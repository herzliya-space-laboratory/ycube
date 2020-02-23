#include <string.h>
#include <stdlib.h>

#include <satellite-subsystems/IsisTRXVU.h>
#include <satellite-subsystems/IsisAntS.h>

#include "GlobalStandards.h"
#include "TRXVU_Commands.h"

extern xTaskHandle xDumpHandle;			 //task handle for dump task
extern xSemaphoreHandle xDumpLock; // this global lock is defined once in TRXVU.c

void DumpTask(void *args) {
	if (NULL == args) {
		FinishDump(NULL, NULL, ACK_DUMP_ABORT, NULL, 0);
		return;
	}
	dump_arguments_t *task_args = (dump_arguments_t *) args;


	/* TODO: add this to readTLMFile function
	sat_packet_t dump_tlm = { 0 };

	SendAckPacket(ACK_DUMP_START, task_args->cmd,
			(unsigned char*) &num_of_elements, sizeof(num_of_elements));

	AssembleCommand((unsigned char*)buffer, size_of_element,
			(char) DUMP_SUBTYPE, (char) (task_args->dump_type),
			task_args->cmd->ID, &dump_tlm);


	TransmitSplPacket(&dump_tlm, &availFrames);

	FinishDump(task_args, buffer, ACK_DUMP_FINISHED, NULL, 0);

	*/

	readTLMFile(task_args->dump_type,task_args->day,task_args->numberOfDays);
}


int CMD_StartDump(sat_packet_t *cmd)
{
	if (NULL == cmd) {
		return -1;
	}

	dump_arguments_t *dmp_pckt = malloc(sizeof(*dmp_pckt));
	unsigned int offset = 0;

	dmp_pckt->cmd = cmd;

	memcpy(&dmp_pckt->dump_type, cmd->data, sizeof(dmp_pckt->dump_type));
	offset += sizeof(dmp_pckt->dump_type);

	memcpy(&dmp_pckt->t_start, cmd->data + offset, sizeof(dmp_pckt->t_start));
	offset += sizeof(dmp_pckt->t_start);

	memcpy(&dmp_pckt->t_end, cmd->data + offset, sizeof(dmp_pckt->t_end));
	offset += sizeof(dmp_pckt->t_end);

	memcpy(&dmp_pckt->day, cmd->data + offset, sizeof(dmp_pckt->day));
	offset += sizeof(dmp_pckt->day);

	memcpy(&dmp_pckt->numberOfDays, cmd->data + offset, sizeof(dmp_pckt->numberOfDays));



	if (xSemaphoreTake(xDumpLock,SECONDS_TO_TICKS(1)) != pdTRUE) {
		return E_GET_SEMAPHORE_FAILED;
	}
	xTaskCreate(DumpTask, (const signed char* const )"DumpTask", 2000,
			(void* )dmp_pckt, configMAX_PRIORITIES - 2, xDumpHandle);

	return 0;
}



int CMD_SendDumpAbortRequest(sat_packet_t *cmd)
{
	(void)cmd;
	SendDumpAbortRequest();
	return 0;
}

int CMD_ForceDumpAbort(sat_packet_t *cmd)
{
	(void)cmd;
	AbortDump();
	return 0;
}


int CMD_MuteTRXVU(sat_packet_t *cmd)
{
	int err = 0;
	time_unix mute_duaration = 0;
	memcpy(&mute_duaration,cmd->data,sizeof(mute_duaration));
	err = muteTRXVU(mute_duaration);
	return err;
}

int CMD_UnMuteTRXVU(sat_packet_t *cmd)
{
	(void)cmd;
	UnMuteTRXVU();
	return 0;
}

int CMD_GetBaudRate(sat_packet_t *cmd)
{
	//ISIStrxvuBitrateStatus bitrate;
	ISIStrxvuTransmitterState trxvu_state;
	if (logError(IsisTrxvu_tcGetState(ISIS_TRXVU_I2C_BUS_INDEX, &trxvu_state))) return -1;

	int bitrate = trxvu_state.fields.transmitter_bitrate;
	TransmitDataAsSPL_Packet(cmd, (unsigned char*) &bitrate, sizeof(bitrate));

	return 0;
}



int CMD_GetBeaconInterval(sat_packet_t *cmd)
{
	int err = 0;
	time_unix beacon_interval = 0;
	err = FRAM_read((unsigned char*) &beacon_interval,
			BEACON_INTERVAL_TIME_ADDR,
			BEACON_INTERVAL_TIME_SIZE);
	TransmitDataAsSPL_Packet(cmd, (unsigned char*) &beacon_interval,
			sizeof(beacon_interval));
	return err;
}

int CMD_SetBeaconInterval(sat_packet_t *cmd)
{
	int err = 0;
	time_unix interval = 0;
	err =  FRAM_write((unsigned char*) &cmd->data,
			BEACON_INTERVAL_TIME_ADDR,
			BEACON_INTERVAL_TIME_SIZE);

	err += FRAM_read((unsigned char*) &interval,
			BEACON_INTERVAL_TIME_ADDR,
			BEACON_INTERVAL_TIME_SIZE);

	TransmitDataAsSPL_Packet(cmd, (unsigned char*) &interval,sizeof(interval));
	return err;
}

int CMD_SetBaudRate(sat_packet_t *cmd)
{
	// TODO do want to save the new bit rate in the FRAME so after restart (init_logic) we will continute to use the new rate??
	ISIStrxvuBitrateStatus bitrate;
	bitrate = (ISIStrxvuBitrateStatus) cmd->data[0]; // TODO why do we take only the first byte?? we need more, no? to get 9600 for example.
	return IsisTrxvu_tcSetAx25Bitrate(ISIS_TRXVU_I2C_BUS_INDEX, bitrate);
}


int CMD_GetTxUptime(sat_packet_t *cmd)
{
	int err = 0;
	time_unix uptime = 0;
	err = IsisTrxvu_tcGetUptime(ISIS_TRXVU_I2C_BUS_INDEX, (unsigned int*)&uptime);
	TransmitDataAsSPL_Packet(cmd, (unsigned char*)&uptime, sizeof(uptime));

	return err;
}

int CMD_GetRxUptime(sat_packet_t *cmd)
{
	int err = 0;
	time_unix uptime = 0;
	err = IsisTrxvu_rcGetUptime(ISIS_TRXVU_I2C_BUS_INDEX,(unsigned int*) &uptime);
	TransmitDataAsSPL_Packet(cmd, (unsigned char*) &uptime, sizeof(uptime));

	return err;
}

int CMD_GetNumOfDelayedCommands(sat_packet_t *cmd)
{
	int err = 0;
	unsigned char temp = 0;
	temp = GetDelayedCommandBufferCount();
	TransmitDataAsSPL_Packet(cmd, (unsigned char*) &temp, sizeof(temp));

	return err;
}

int CMD_GetNumOfOnlineCommands(sat_packet_t *cmd)
{
	int err = 0;
	unsigned short int temp = 0;
	err = IsisTrxvu_rcGetFrameCount(ISIS_TRXVU_I2C_BUS_INDEX, &temp);
	TransmitDataAsSPL_Packet(cmd, (unsigned char*) &temp, sizeof(temp));

	return err;
}

int CMD_DeleteDelyedCmdByID(sat_packet_t *cmd)
{
	int err = 0;
	unsigned int index = 0;
	memcpy(&index,cmd->data,sizeof(index));
	err = DeleteDelayedCommandByIndex(index);
	return err;
}

int CMD_DeleteAllDelyedBuffer(sat_packet_t *cmd)
{
	(void)cmd;
	int err = 0;
	err = DeleteDelayedBuffer();
	return err;
}

int CMD_AntSetArmStatus(sat_packet_t *cmd)
{
	if (cmd == NULL || cmd->data == NULL) {
		return E_INPUT_POINTER_NULL;
	}
	int err = 0;
	ISISantsSide ant_side = cmd->data[0];

	ISISantsArmStatus status = cmd->data[1];
	err = IsisAntS_setArmStatus(ISIS_TRXVU_I2C_BUS_INDEX, ant_side, status);

	return err;
}

int CMD_AntGetArmStatus(sat_packet_t *cmd)
{
	int err = 0;
	ISISantsStatus status;
	ISISantsSide ant_side;
	memcpy(&ant_side, cmd->data, sizeof(ant_side));

	err = IsisAntS_getStatusData(ISIS_TRXVU_I2C_BUS_INDEX, ant_side, &status);
	TransmitDataAsSPL_Packet(cmd, (unsigned char*) &status, sizeof(status));

	return err;
}

int CMD_AntGetUptime(sat_packet_t *cmd)
{
	int err = 0;
	time_unix uptime = 0;
	ISISantsSide ant_side;
	memcpy(&ant_side, cmd->data, sizeof(ant_side));
	err = IsisAntS_getUptime(ISIS_TRXVU_I2C_BUS_INDEX, ant_side,(unsigned int*) &uptime);
	TransmitDataAsSPL_Packet(cmd, (unsigned char*) &uptime, sizeof(uptime));
	return err;
}

int CMD_AntCancelDeployment(sat_packet_t *cmd)
{
	int err = 0;
	ISISantsSide ant_side;
	memcpy(&ant_side, cmd->data, sizeof(ant_side));
	err = IsisAntS_cancelDeployment(ISIS_TRXVU_I2C_BUS_INDEX, ant_side);
	return err;
}




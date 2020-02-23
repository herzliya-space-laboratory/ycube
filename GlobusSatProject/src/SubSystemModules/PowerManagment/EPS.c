
#include <satellite-subsystems/IsisSolarPanelv2.h>
#include <hal/errors.h>
#include <utils.h>
#include <string.h>

#include "EPS.h"
#ifdef ISISEPS
	#include <satellite-subsystems/isis_eps_driver.h>
#endif
#ifdef GOMEPS
	#include <satellite-subsystems/GomEPS.h>
#endif

// y[i] = a * x[i] +(1-a) * y[i-1]
voltage_t prev_avg = 0;		// y[i-1]
float alpha = 0;			//<! smoothing constant


// holds all 6 default values for eps_threshold
EpsThreshVolt_t eps_threshold_voltages = {.raw = DEFAULT_EPS_THRESHOLD_VOLTAGES};	// saves the current EPS logic threshold voltages

int GetBatteryVoltage(voltage_t *vbatt)
{
	isis_eps__gethousekeepingengincdb__from_t hk_tlm;

	if(logError(isis_eps__gethousekeepingengincdb__tm(EPS_I2C_BUS_INDEX, &hk_tlm)))return -1;

	*vbatt = hk_tlm.fields.batt_input.fields.volt;

	return 0;
}

int EPS_Init()
{
	unsigned char i2c_address3 = EPS_I2C_ADDR;
	ISIS_EPS_t i2c_address[1];
	if(logError(ISIS_EPS_Init(&i2c_address , 1))) return -1;


	if(logError(IsisSolarPanelv2_initialize(slave0_spi))) return -1;
//	IsisSolarPanelv2_sleep(); cheek


	if(GetThresholdVoltages(&eps_threshold_voltages)) return -1;


	if(GetAlpha(&alpha)){
		alpha = DEFAULT_ALPHA_VALUE;
	}


	prev_avg = 0;
	GetBatteryVoltage(&prev_avg);


	EPS_Conditioning();

	return 0;
}

#define GetFilterdVoltage(curr_voltage) (voltage_t) (alpha * curr_voltage + (1 - alpha) * prev_avg)

int EPS_Conditioning()
{
	voltage_t curr_voltage = 0;

	GetBatteryVoltage(&curr_voltage);

	voltage_t filtered_voltage = 0;					// the currently filtered voltage; y[i]
	filtered_voltage = GetFilterdVoltage(curr_voltage);

	if(filtered_voltage < prev_avg){
		if(filtered_voltage  <  eps_threshold_voltages.fields.Vdown_safe ){
			 EnterCriticalMode();
		 }else if(filtered_voltage < eps_threshold_voltages.fields.Vdown_cruise){
			 EnterSafeMode();
		 }else if(filtered_voltage < eps_threshold_voltages.fields.Vdown_full){
			 EnterCruiseMode();
		 }

		}else if(filtered_voltage > prev_avg){
			if(filtered_voltage > eps_threshold_voltages.fields.Vup_full){
				EnterFullMode();
			}else if(filtered_voltage > eps_threshold_voltages.fields.Vup_cruise){
				EnterCruiseMode();
			}else if(filtered_voltage > eps_threshold_voltages.fields.Vup_safe){
				EnterSafeMode();
			}
		prev_avg = filtered_voltage;
		}
	return 0;
}

int UpdateAlpha(float new_alpha)
{
	if(new_alpha < 0 || new_alpha > 1){
		return logError(-2);
	}

	if(logError(FRAM_write((unsigned char*) &new_alpha , EPS_ALPHA_FILTER_VALUE_ADDR , EPS_ALPHA_FILTER_VALUE_SIZE)))return -1;
	GetAlpha(&alpha);
	return 0;
}

int UpdateThresholdVoltages(EpsThreshVolt_t *thresh_volts)
{
	if(NULL == thresh_volts){
		return logError(E_INPUT_POINTER_NULL);
	}

	Boolean valid_dependancies = (thresh_volts->fields.Vup_safe 	< thresh_volts->fields.Vup_cruise
		                           && thresh_volts->fields.Vup_cruise	< thresh_volts->fields.Vup_full);

		Boolean valid_regions = (thresh_volts->fields.Vdown_full 	< thresh_volts->fields.Vup_full)
							&&  (thresh_volts->fields.Vdown_cruise	< thresh_volts->fields.Vup_cruise)
							&&  (thresh_volts->fields.Vdown_safe	< thresh_volts->fields.Vup_safe);

		if (!(valid_dependancies && valid_regions)) {
			return logError(-2);
		}

	if(logError(FRAM_write((unsigned char*) thresh_volts , EPS_THRESH_VOLTAGES_ADDR , EPS_THRESH_VOLTAGES_SIZE)))return -1;

	GetThresholdVoltages(&eps_threshold_voltages);


	return 0;
}

// check: what happens first time when there are no values in the FRAM
int GetThresholdVoltages(EpsThreshVolt_t thresh_volts[NUMBER_OF_THRESHOLD_VOLTAGES])
{
	if(NULL == thresh_volts){
		return logError(E_INPUT_POINTER_NULL);
	}

	if(logError(FRAM_read((unsigned char*) thresh_volts , EPS_THRESH_VOLTAGES_ADDR , EPS_THRESH_VOLTAGES_SIZE)))return -1;
	return 0;
}

int GetAlpha(float *alpha)
{
	if(NULL == alpha){
		return logError(E_INPUT_POINTER_NULL);
	}
	if(logError(FRAM_read((unsigned char*) (unsigned char*) alpha , EPS_ALPHA_FILTER_VALUE_ADDR , EPS_ALPHA_FILTER_VALUE_SIZE)))return -1;

	return 0;

}

int RestoreDefaultAlpha()
{
	float def_alpha = DEFAULT_ALPHA_VALUE;
	if(logError(UpdateAlpha(def_alpha)))return -1;
	return 0;
}

int RestoreDefaultThresholdVoltages()
{
	EpsThreshVolt_t def_thresh = {.raw = DEFAULT_EPS_THRESHOLD_VOLTAGES};
	if(logError(UpdateThresholdVoltages(&def_thresh)))return -1;
	return 0;
}


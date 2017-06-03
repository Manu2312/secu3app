/* SECU-3  - An open source, free engine control unit
   Copyright (C) 2007 Alexey A. Shabelnikov. Ukraine, Kiev

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.

   contacts:
              http://secu-3.org
              email: shabelnikov@secu-3.org
*/

/** \file secu3.c
 * \author Alexey A. Shabelnikov
 * Implementation of main module of the firmware.
 * (���������� �������� ������ ��������).
 */

#include "port/avrio.h"
#include "port/intrinsic.h"
#include "port/port.h"

#include "adc.h"
#include "bc_input.h"
#include "bitmask.h"
#include "bluetooth.h"
#include "bootldr.h"
#include "camsens.h"
#include "carb_afr.h"
#include "ce_errors.h"
#include "choke.h"
#include "ckps.h"
#include "crc16.h"
#include "diagnost.h"
#include "ecudata.h"
#include "eculogic.h"
#include "eeprom.h"
#include "gasdose.h"
#include "pwrvalve.h"
#include "fuelpump.h"
#include "funconv.h"
#include "jumper.h"
#include "fuelcut.h"
#include "immobiliz.h"
#include "injector.h"
#include "intkheat.h"
#include "ioconfig.h"
#include "knklogic.h"
#include "knock.h"
#include "lambda.h"
#include "magnitude.h"
#include "measure.h"
#include "mathemat.h"
#include "params.h"
#include "procuart.h"
#include "pwrrelay.h"
#include "starter.h"
#include "suspendop.h"
#include "tables.h"
#include "uart.h"
#include "uni_out.h"
#include "ventilator.h"
#include "vstimer.h"
#include "wdt.h"

#define FORCE_MEASURE_TIMEOUT_VALUE   20    //!< timeout value used to perform measurements when engine is stopped
#if defined(HALL_SYNC) || defined(CKPS_NPLUS1)
#define ENGINE_ROTATION_TIMEOUT_VALUE 150   //!< timeout value used to determine that engine is stopped (used for Hall sensor)
#else
#define ENGINE_ROTATION_TIMEOUT_VALUE 20    //!< timeout value used to determine that engine is stopped (this value must not exceed 25)
#endif

/**Control of certain units of engine (���������� ���������� ������ ���������).
 * \param d pointer to ECU data structure
 */
void control_engine_units(struct ecudata_t *d)
{
#if !defined(CARB_AFR) || defined(GD_CONTROL) //Carb. AFR control supersede idle cut-off functionality
 //Idle fuel cut-off control or fuel cut-off
 fuelcut_control(d);
#endif

 //Starter blocking control
 starter_control(d);

 //���������� ������� ������������ ���������� ���������, ��� ������� ��� ���� ������������ � �������
 vent_control(d);

#ifndef CARB_AFR //Carb. AFR control supersede power valve functionality
 //Power valve control
 pwrvalve_control(d);
#endif

#ifdef FUEL_PUMP
 //Controlling of electric fuel pump (���������� �������������������)
 fuelpump_control(d);
#endif

 //power management
 pwrrelay_control(d);

#if defined(SM_CONTROL) || defined(FUEL_INJECT)
 //choke control
 choke_control(d);
#endif

#if defined(GD_CONTROL)
 //gas dosator control
 gasdose_control(d);
#endif

 //Cam sensor control
 cams_control();

#ifdef INTK_HEATING
 intkheat_control(d);
#endif

#ifdef UNI_OUTPUT
 //Universal programmable output control
 uniout_control(d);
#endif

#if defined(FUEL_INJECT) || defined(CARB_AFR) || defined(GD_CONTROL)
 lambda_control(d);
#endif

#ifdef CARB_AFR
 //Carburetor AFR control
 carbafr_control(d);
#endif
}

/** Check firmware integrity (CRC) and set error indication if code or data is damaged
 */
void check_firmware_integrity(void)
{
 if (crc16f(0, CODE_SIZE)!=PGM_GET_WORD(&fw_data.code_crc))
  ce_set_error(ECUERROR_PROGRAM_CODE_BROKEN);
 if (crc16f(fwinfo, FWINFOSIZE)!=0x44DB)
  check_firmware_integrity(); //Uuups!
}

/**Initialization of I/O ports
 */
void init_ports(void)
{
 jumper_init_ports();  //<--must be first!
 ckps_init_ports();
 cams_init_ports();
 vent_init_ports();
#ifndef CARB_AFR //Carb. AFR control supersede power valve functionality
 pwrvalve_init_ports();
#endif
#ifdef FUEL_PUMP
 fuelpump_init_ports();
#endif
#if !defined(CARB_AFR) || defined(GD_CONTROL) //Carb. AFR control supersede idle cut-off functionality
 fuelcut_init_ports();
#endif
 starter_init_ports();
 ce_init_ports();
 knock_init_ports();
 pwrrelay_init_ports();
#if defined(SM_CONTROL) || defined(FUEL_INJECT)
 choke_init_ports();
#endif
#if defined(GD_CONTROL)
 gasdose_init_ports();
#endif
#ifdef INTK_HEATING
 intkheat_init_ports();
#endif
 meas_init_ports();
#ifdef UNI_OUTPUT
 uniout_init_ports();
#endif
#ifdef FUEL_INJECT
 inject_init_ports();
#endif
#ifdef CARB_AFR
 carbafr_init_ports();
#endif
}

/**Initialization of system modules
 */
void init_modules(void)
{
 //��������������� ������������� ���������� ����������� ���������� ���������
 knock_set_band_pass(edat.param.knock_bpf_frequency);
 knock_set_gain(PGM_GET_BYTE(&fw_data.exdata.attenuator_table[0]));
 knock_set_int_time_constant(edat.param.knock_int_time_const);
 knock_set_channel(0);
 if (edat.param.knock_use_knock_channel)
  if (!knock_module_initialize())
  {//��� ����������� ���������� ��������� ���������� - �������� ��
   ce_set_error(ECUERROR_KSP_CHIP_FAILED);
  }
 edat.use_knock_channel_prev = edat.param.knock_use_knock_channel;

 //Initialization of ADC
 adc_init();


 //Take away of starter blocking (������� ���������� ��������)
 starter_set_blocking_state(0);

 //Initialization of UART (�������������� UART)
 uart_init(edat.param.uart_divisor);

#ifdef BLUETOOTH_SUPP
 //Initialization of Bluetooth related module
 bt_init(edat.param.bt_flags & (1 << 1));
#endif

 //initialization of cam module, must precede ckps initialization
 cams_init_state();

#ifdef FUEL_PUMP
 //initialization of electric fuel pump
 fuelpump_init();
#endif

 //initialization of power management unit
 pwrrelay_init();

#if defined(SM_CONTROL) || defined(FUEL_INJECT)
 choke_init();
#endif

#if defined(GD_CONTROL)
 gasdose_init();
#endif

 //initialization of intake manifold heating unit
#ifdef INTK_HEATING
 intkheat_init();
#endif

 //initialization of an universal programmable output
#ifdef UNI_OUTPUT
 uniout_init();
#endif

 //�������������� ������ ����
 ckps_init_state();
 ckps_set_cyl_number(edat.param.ckps_engine_cyl);
 ckps_set_cogs_num(edat.param.ckps_cogs_num, edat.param.ckps_miss_num);
 ckps_set_edge_type(edat.param.ckps_edge_type);     //CKPS edge (����� ����)
 cams_vr_set_edge_type(edat.param.ref_s_edge_type); //REF_S edge (����� ���)
 ckps_set_cogs_btdc(edat.param.ckps_cogs_btdc); //<--only partial initialization
#ifndef DWELL_CONTROL
 ckps_set_ignition_cogs(edat.param.ckps_ignit_cogs);
#else
 ckps_set_rising_spark(CHECKBIT(edat.param.hall_flags, CKPF_RISING_SPARK));
#endif
 ckps_set_knock_window(edat.param.knock_k_wnd_begin_angle,edat.param.knock_k_wnd_end_angle);
 ckps_use_knock_channel(edat.param.knock_use_knock_channel);
 ckps_set_cogs_btdc(edat.param.ckps_cogs_btdc); //<--now valid initialization
 ckps_set_merge_outs(edat.param.merge_ign_outs);
#ifdef HALL_OUTPUT
 ckps_set_hall_pulse(edat.param.hop_start_cogs, edat.param.hop_durat_cogs);
#endif
#if defined(HALL_SYNC) || defined(CKPS_NPLUS1)
 ckps_set_shutter_wnd_width(edat.param.hall_wnd_width);
 ckps_set_advance_angle(0);
#endif

#ifdef FUEL_INJECT
 ckps_set_inj_timing(edat.param.inj_timing_crk); //use inj.timing on cranking
 inject_init_state();
 inject_set_cyl_number(edat.param.ckps_engine_cyl);
 inject_set_num_squirts(edat.param.inj_config & 0xF);
 inject_set_fuelcut(!edat.sys_locked);
 inject_set_config(edat.param.inj_config >> 4);
#endif
#if defined(FUEL_INJECT) || defined(CARB_AFR) || defined(GD_CONTROL)
 lambda_init_state();
#endif

#ifdef CARB_AFR
 carbafr_init();
#endif

 s_timer_init();
 ignlogic_init();

 vent_init_state();
 vent_set_pwmfrq(edat.param.vent_pwmfrq);

 //check and enter blink codes indication mode
 bc_indication_mode(&edat);

 //Initialization of the suspended operations module
 sop_init_operations();

 //�������� ��������� ������ ��������� �������� ��� ������������� ������
 meas_initial_measure(&edat);
}

/**Main function of firmware - entry point. Contains initialization and main loop 
 * (������� ������� � ���� ��������. � ��� ���������� ���������� ���������, ��� ��������
 * ��� ������������� � ������� ����)
 */
MAIN()
{
 int16_t calc_adv_ang = 0;
 uint8_t turnout_low_priority_errors_counter = 255;
 int16_t advance_angle_inhibitor_state = 0;
 retard_state_t retard_state;

 //We need this because we might been reset by WDT
 wdt_turnoff_timer();

 //���������� ��������� ������ ���������� ��������� �������
 init_ecu_data();
 knklogic_init(&retard_state);

 //Perform I/O ports configuration/initialization (������������� ����� �����/������)
 init_ports();

 //If firmware code is damaged then turn on CE (���� ��� ��������� �������� - �������� ��)
 check_firmware_integrity();

 //Start watchdog timer! (��������� ���������� ������)
 wdt_start_timer();

 //Read all system parameters (������ ��� ��������� �������)
 load_eeprom_params(&edat);

#ifdef IMMOBILIZER
 //If enabled, reads and checks security keys, performs system lock/unlock
 immob_check_state(&edat);
#endif

#ifdef REALTIME_TABLES
 //load tables' set from EEPROM into RAM
 load_specified_tables_into_ram(&edat, TABLES_NUMBER - 1);
#endif

 //perform initialization of all system modules
 init_modules();

 //Enable all interrupts globally before we fall in main loop
 //��������� ��������� ���������� ����� �������� ��������� ����� ���������
 _ENABLE_INTERRUPT();

 //------------------------------------------------------------------------
 while(1)
 {
  if (ckps_is_cog_changed())
   s_timer_set(engine_rotation_timeout_counter, ENGINE_ROTATION_TIMEOUT_VALUE);

  if (s_timer_is_action(engine_rotation_timeout_counter))
  { //��������� ����������� (��� ������� ���� �����������)
#ifdef DWELL_CONTROL
   ckps_init_ports();           //����� IGBT �� ������� � �������� ���������
   //TODO: ������� ������ ������� ��� ���������� �� ������������� �����. ���?
#endif
   ckps_init_state_variables();
   cams_init_state_variables();
   edat.engine_mode = EM_START; //����� �����

   knklogic_init(&retard_state);

   if (edat.param.knock_use_knock_channel)
    knock_start_settings_latching();

   edat.corr.curr_angle = calc_adv_ang;
   meas_update_values_buffers(&edat, 1, &fw_data.exdata.cesd);  //<-- update RPM only
  }

  //��������� ��������� ���, ����� ������ ���������� �������. ��� ����������� ������� ��������
  //����� ���� ������ ��������������������. ����� �������, ����� ������� �������� ��������� ��������
  //������������ ��������, �� ��� ������� ���������� �����������.
  if (s_timer_is_action(force_measure_timeout_counter))
  {
   if (!edat.param.knock_use_knock_channel)
   {
    _DISABLE_INTERRUPT();
    adc_begin_measure(0);  //normal speed
    _ENABLE_INTERRUPT();
   }
   else
   {
    //���� ������ ���������� �������� �������� � HIP, �� ����� ��������� �� ����������.
    while(!knock_is_latching_idle());
    _DISABLE_INTERRUPT();
    //�������� ����� �������������� � ���� ����� 20���, ���� ���������� ������ ������������� (����������
    //�� ��� ������ ������ �� ��������). � ������ ������ ��� ������ ��������� � ���, ��� �� ������ ����������
    //������������ 20-25���, ��� ��� ��� ���������� �� ����� ��������� ��������.
    knock_set_integration_mode(KNOCK_INTMODE_INT);
    _DELAY_US(22);
    knock_set_integration_mode(KNOCK_INTMODE_HOLD);
    adc_begin_measure_all(); //�������� ������ � �� ����
    _ENABLE_INTERRUPT();
   }

   s_timer_set(force_measure_timeout_counter, FORCE_MEASURE_TIMEOUT_VALUE);
   meas_update_values_buffers(&edat, 0, &fw_data.exdata.cesd);
  }

  //----------����������� ����������-----------------------------------------
  //���������� ���������� ��������
  sop_execute_operations(&edat);
  //���������� ������������� � �������������� ����������� ������
  ce_check_engine(&edat, &ce_control_time_counter);
  //��������� ����������/�������� ������ ����������������� �����
  process_uart_interface(&edat);
  //���������� ����������� ��������
  save_param_if_need(&edat);
  //������ ���������� ������� �������� ���������
  edat.sens.inst_frq = ckps_calculate_instant_freq();
  //���������� ���������� ������� ���������� � ��������� �������
  meas_average_measured_values(&edat, &fw_data.exdata.cesd);
  //c�������� ���������� ����� ������� � ����������� ��� �������
  meas_take_discrete_inputs(&edat);
  //���������� ����������
  control_engine_units(&edat);
  //�� ��������� ������� (��������� ������� - ������ ��������� �����)
  calc_adv_ang = ignlogic_system_state_machine(&edat);
  //��������� � ��� �����-���������
  calc_adv_ang+=edat.param.angle_corr;
  //------------------------------
  edat.corr.octan_aac = edat.param.angle_corr;
  //------------------------------
  //������������ ������������ ��� �������������� ���������
  restrict_value_to(&calc_adv_ang, edat.param.min_angle, edat.param.max_angle);
  //���� ����� ����� �������� ���, �� 0
  if (edat.param.zero_adv_ang)
   calc_adv_ang = 0;

#ifdef DWELL_CONTROL
#if defined(HALL_SYNC) || defined(CKPS_NPLUS1)
  //Double dwell time if RPM is low and non-stable
  ckps_set_acc_time(edat.st_block ? accumulation_time(&edat) : accumulation_time(&edat) << 1);
#else
  //calculate and update accumulation time (dwell control)
  ckps_set_acc_time(accumulation_time(&edat));
#endif
#endif
  if (edat.sys_locked)
   ckps_enable_ignition(0);
  else
  {
   //���� ���������, �� ������ ������� ��������� ��� ���������� ������������ ��������
   if (edat.param.ign_cutoff)
    ckps_enable_ignition(edat.sens.inst_frq < edat.param.ign_cutoff_thrd);
   else
    ckps_enable_ignition(1);
  }

#ifdef DIAGNOSTICS
  diagnost_process(&edat);
#endif
  //------------------------------------------------------------------------


  //��������� �������� ������� ���������� ��������� ������ ��� ������� �������� �����.
  if (ckps_is_stroke_event_r())
  {
   meas_update_values_buffers(&edat, 0, &fw_data.exdata.cesd);
   s_timer_set(force_measure_timeout_counter, FORCE_MEASURE_TIMEOUT_VALUE);

   //������������ ������� ��������� ���, �� �� ����� ���������� ������ ��� �� ������������ ��������
   //�� ���� ������� ����. � ������ ����� ������ ��� ��������.
   if (EM_START == edat.engine_mode)
   {
#if defined(HALL_SYNC) || defined(CKPS_NPLUS1)
    int16_t strt_map_angle = start_function(&edat);
    ckps_set_shutter_spark(0==strt_map_angle);
    edat.corr.curr_angle = advance_angle_inhibitor_state = (0==strt_map_angle ? 0 : calc_adv_ang);
#else
    edat.corr.curr_angle = advance_angle_inhibitor_state = calc_adv_ang;
#endif
   }
   else
   {
#if defined(HALL_SYNC) || defined(CKPS_NPLUS1)
    ckps_set_shutter_spark(edat.sens.frequen < 200);
#endif
    edat.corr.curr_angle = advance_angle_inhibitor(calc_adv_ang, &advance_angle_inhibitor_state, edat.param.angle_inc_speed, edat.param.angle_dec_speed);
   }

   //----------------------------------------------
   if (edat.param.knock_use_knock_channel)
   {
    knklogic_detect(&edat, &retard_state);
    knklogic_retard(&edat, &retard_state);
   }
   else
    edat.corr.knock_retard = 0;
   //----------------------------------------------

   //��������� ��� ��� ���������� � ��������� �� ������� ����� ���������
   ckps_set_advance_angle(edat.corr.curr_angle);

#ifdef FUEL_INJECT
   //set current injection time and fuel cut state
   inject_set_inj_time(edat.inj_pw);
#ifdef GD_CONTROL
   //enable/disable fuel supply depending on fuel cut, rev.lim, sys.lock flags. Also fuel supply will be disabled if fuel type is gas and gas doser is activated
   inject_set_fuelcut(edat.ie_valve && !edat.sys_locked && !edat.fc_revlim && pwrrelay_get_state() && !(edat.sens.gas && (IOCFG_CHECK(IOP_GD_STP) || CHECKBIT(edat.param.flpmp_flags, FPF_INJONGAS))));
#else
   inject_set_fuelcut(edat.ie_valve && !edat.sys_locked && !edat.fc_revlim && pwrrelay_get_state() && !(edat.sens.gas && CHECKBIT(edat.param.flpmp_flags, FPF_INJONGAS)));
#endif
   //set injection timing depending on current mode of engine
   ckps_set_inj_timing(edat.corr.inj_timing);
#endif
#if defined(FUEL_INJECT) || defined(CARB_AFR) || defined(GD_CONTROL)
   lambda_stroke_event_notification(&edat);
#endif

   ignlogic_stroke_event_notification(&edat);

#ifdef GD_CONTROL
   gasdose_stroke_event_notification(&edat);
#endif

   //��������� ��������� ����������� � ����������� �� ��������
   if (edat.param.knock_use_knock_channel)
    knock_set_gain(knock_attenuator_function(&edat));

   // ������������� ���� ������ ���������� ��� ������ �������� ���������
   //(��� ���������� N-�� ���������� ������)
   if (turnout_low_priority_errors_counter == 1)
   {
    ce_clear_error(ECUERROR_EEPROM_PARAM_BROKEN);
    ce_clear_error(ECUERROR_PROGRAM_CODE_BROKEN);
   }
   if (turnout_low_priority_errors_counter > 0)
    turnout_low_priority_errors_counter--;
  }

  wdt_reset_timer();
 }//main loop
 //------------------------------------------------------------------------
}

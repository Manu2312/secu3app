 /****************************************************************
 *       SECU-3  - An open source, free engine control unit
 *    Designed by Alexey A. Shabelnikov. Ukraine, Gorlovka 2007.
 *       Microprocessors systems - design & programming.
 *    contacts:
 *              http://secu-3.narod.ru
 *              ICQ: 405-791-931
 ****************************************************************/

#include <inavr.h>
#include <iom16.h>
#include "bitmask.h"
#include "adc.h"
#include "main.h"


//������������� ���� ������ ���������� ���������� ����������-������:
// f1.adc_sensors_ready

typedef struct
{
 unsigned int map_abuf[MAP_AVERAGING];           //����� ���������� ����������� ��������
 unsigned int bat_abuf[BAT_AVERAGING];           //����� ���������� ���������� �������� ����
 unsigned int tmp_abuf[TMP_AVERAGING];           //����� ���������� ����������� ����������� ��������

 unsigned char  map_ai;
 unsigned char  bat_ai;
 unsigned char  tmp_ai;      
}adc_state;

adc_state adc;  //���������� ��������� ���


unsigned int adc_get_map_value(unsigned char index)
{
  return adc.map_abuf[index];
}

unsigned int adc_get_ubat_value(unsigned char index)
{
  return adc.bat_abuf[index];
}

unsigned int adc_get_temp_value(unsigned char index)
{
  return adc.tmp_abuf[index];
}


void adc_begin_measure(void) 
{ 
  //�� �� ����� ��������� ����� ���������, ���� ��� �� �����������
  //���������� ���������
  if (!f1.adc_sensors_ready)  
    return;

  f1.adc_sensors_ready = 0; 
  ADMUX = ADCI_MAP|ADC_VREF_TYPE; 
  SETBIT(ADCSRA,ADSC);
}  

char adc_is_measure_ready(void)
{
  return f1.adc_sensors_ready; 
}

//������������� ��� � ��� ���������� ���������
void adc_init(void)
{
 adc.map_ai = MAP_AVERAGING-1;
 adc.bat_ai = BAT_AVERAGING-1;
 adc.tmp_ai = TMP_AVERAGING-1;      
 
 //������������� ���, ���������: f = 125.000 kHz, 
 //���������� �������� �������� ���������� - 2.56V, ���������� ��������� 
 ADMUX=ADC_VREF_TYPE;
 ADCSRA=(1<<ADEN)|(1<<ADIE)|(1<<ADPS2)|(1<<ADPS1)|(1<<ADPS0);     

 //������ ��� ����� � ������ ���������
 f1.adc_sensors_ready = 1;
}

//���������� �� ���������� �������������� ���. ��������� �������� ���� ���������� ��������. ����� �������
//��������� ��� ���������� ����� ��������� ��� ������� �����, �� ��� ��� ���� ��� ����� �� ����� ����������.
#pragma vector=ADC_vect
__interrupt void ADC_isr(void)
{
 __enable_interrupt(); 

 switch(ADMUX&0x07)
 {
   case ADCI_MAP: //��������� ��������� ����������� ��������
      adc.map_abuf[adc.map_ai] = ADC;      

      //��������� �������� ������� ������ ����������
      (adc.map_ai==0) ? (adc.map_ai = MAP_AVERAGING - 1): adc.map_ai--;            

      ADMUX = ADCI_UBAT|ADC_VREF_TYPE;   
      SETBIT(ADCSRA,ADSC);
      break;

   case ADCI_UBAT://��������� ��������� ���������� �������� ����
      adc.bat_abuf[adc.bat_ai] = ADC;      

      //��������� �������� ������� ������ ����������
      (adc.bat_ai==0) ? (adc.bat_ai = BAT_AVERAGING - 1): adc.bat_ai--;            

      ADMUX = ADCI_TEMP|ADC_VREF_TYPE;   
      SETBIT(ADCSRA,ADSC);
      break;

   case ADCI_TEMP://��������� ��������� ����������� ����������� ��������
      adc.tmp_abuf[adc.tmp_ai] = ADC;      

      //���������  �������� ������� ������ ����������
      (adc.tmp_ai==0) ? (adc.tmp_ai = TMP_AVERAGING - 1): adc.tmp_ai--;               

      ADMUX = ADCI_MAP|ADC_VREF_TYPE;    
      f1.adc_sensors_ready = 1;                
      break; 
 } 
}
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

/** \file fuelcut.h
 * \author Alexey A. Shabelnikov
 * Control of Idle Cut-off valve (Carburetor) or fuel cut (Fuel injection).
 */

#ifndef _IDLECON_H_
#define _IDLECON_H_

#if !defined(CARB_AFR) || defined(GD_CONTROL) //Carb. AFR control supersede idle cut-off functionality

struct ecudata_t;

/** Initialization of used I/O ports */
void fuelcut_init_ports(void);

/** Does control of valve or fuel cut
 * \param d pointer to ECU data structure
 */
void fuelcut_control(struct ecudata_t* d);

#endif //!CARB_AFR || GD_CONTROL

#endif //_IDLECON_H_

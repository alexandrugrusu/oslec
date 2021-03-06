/*
 * SpanDSP - a series of DSP components for telephony
 *
 * power_meter.h
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2003 Steve Underwood
 *
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2, as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * $Id: power_meter.h,v 1.8 2006/10/24 13:45:28 steveu Exp $
 */

#if !defined(_POWER_METER_H_)
#define _POWER_METER_H_

/*! \page power_meter_page Power metering

\section power_meter_page_sec_1 What does it do?
The power metering module implements a simple IIR type running power meter. The damping
factor of the IIR is selectable when the meter instance is created.

Note that the definition of dBOv is quite vague in most places - is it peak since wave,
peak square wave, etc.? This code is based on the well defined wording in RFC3389:

"For example, in the case of a u-law system, the reference would be a square wave with
values +/-8031, and this square wave represents 0dBov.  This translates into 6.18dBm0".

\section power_meter_page_sec_2 How does it work?
*/

/*!
    Power meter descriptor. This defines the working state for a
    single instance of a power measurement device.
*/
typedef struct
{
    int shift;

    int32_t reading;
} power_meter_t;

#ifdef __cplusplus
extern "C" {
#endif

/*! Initialise a power meter context.
    \brief Initialise a power meter context.
    \param s The power meter context.
    \param shift The shift to be used by the IIR filter.
    \return The power meter context. */
power_meter_t *power_meter_init(power_meter_t *s, int shift);

/*! Change the damping factor of a power meter context.
    \brief Change the damping factor of a power meter context.
    \param s The power meter context.
    \param shift The new shift to be used by the IIR filter.
    \return The power meter context. */
power_meter_t *power_meter_damping(power_meter_t *s, int shift);

/*! Update a power meter.
    \brief Update a power meter.
    \param s The power meter context.
    \param amp The amplitude of the new audio sample.
    \return The current power meter reading. */
int32_t power_meter_update(power_meter_t *s, int16_t amp);

/*! Get the current power meter reading, in dBm0.
    \brief Get the current power meter reading, in dBm0.
    \param s The power meter context.
    \return The current power meter reading, in dBm0. */
float power_meter_dbm0(power_meter_t *s);

/*! Get the current power meter reading, in dBOv.
    \brief Get the current power meter reading, in dBOv.
    \param s The power meter context.
    \return The current power meter reading, in dBOv. */
float power_meter_dbov(power_meter_t *s);

/*! Get the power meter reading which represents a specified power level in dBm0.
    \brief Get the current power meter reading, in dBm0.
    \param level A power level, in dB0m.
    \return The equivalent power meter reading. */
int32_t power_meter_level_dbm0(float level);

/*! Get the power meter reading which represents a specified power level in dBOv.
    \brief Get the current power meter reading, in dBOv.
    \param level A power level, in dBOv.
    \return The equivalent power meter reading. */
int32_t power_meter_level_dbov(float level);

#ifdef __cplusplus
}
#endif

#endif
/*- End of file ------------------------------------------------------------*/

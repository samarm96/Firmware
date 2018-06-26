/****************************************************************************
 *
 *   Copyright (c) 2018 PX4 Development Team. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name PX4 nor the names of its contributors may be
 *    used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 ****************************************************************************/

/**
 * @file FlightTaskOrbit.cpp
 */

#include "FlightTaskOrbit.hpp"
#include <mathlib/mathlib.h>

using namespace matrix;

FlightTaskOrbit::FlightTaskOrbit()
{
	_sticks_data_required = false;
}

bool FlightTaskOrbit::applyCommandParameters(const vehicle_command_s &command)
{
	const float &r = command.param3; // commanded radius
	const float &v = command.param4; // commanded velocity

	if (setRadius(r) && setVelocity(v)) {
		return FlightTaskManualAltitudeSmooth::applyCommandParameters(command);
	}

	return false;
}

bool FlightTaskOrbit::setRadius(const float r)
{
	if (math::isInRange(r, _radius_min, _radius_max)) {
		// radius is more important than velocity for safety
		if (!checkAcceleration(r, _v, _acceleration_max)) {
			_v = sqrtf(_acceleration_max * r);
		}

		_r = r;
		return true;
	}

	return false;
}

bool FlightTaskOrbit::setVelocity(const float v)
{
	if (fabs(v) < _velocity_max &&
	    checkAcceleration(_r, v, _acceleration_max)) {
		_v = v;
		return true;
	}

	return false;
}

bool FlightTaskOrbit::checkAcceleration(float r, float v, float a)
{
	return v * v < a * r;
}

bool FlightTaskOrbit::activate()
{
	bool ret = FlightTaskManualAltitudeSmooth::activate();
	_r = _radius_min;
	_v =  0.5f;
	_center = Vector2f(_position.data());
	_center(0) -= _r;
	return ret;
}

bool FlightTaskOrbit::update()
{
	// update altitude
	FlightTaskManualAltitudeSmooth::update();

	// stick input adjusts parameters within a fixed time frame
	const float r = _r - _sticks_expo(0) * _deltatime * (_radius_max / 8.f);
	const float v = _v - _sticks_expo(1) * _deltatime * (_velocity_max / 4.f);

	setRadius(r);
	setVelocity(v);

	// xy velocity to go around in a circle
	Vector2f center_to_position = Vector2f(_position.data()) - _center;
	Vector2f velocity_xy = Vector2f(-center_to_position(1), center_to_position(0));
	velocity_xy = velocity_xy.unit_or_zero();
	velocity_xy *= _v;

	// xy velocity adjustment to stay on the radius distance
	velocity_xy += (_r - center_to_position.norm()) * center_to_position.unit_or_zero();

	_velocity_setpoint(0) = velocity_xy(0);
	_velocity_setpoint(1) = velocity_xy(1);

	// make vehicle front always point towards the center
	_yaw_setpoint = atan2f(center_to_position(1), center_to_position(0)) + M_PI_F;
	// yawspeed feed-forward because we know the necessary angular rate
	_yawspeed_setpoint = _v / _r;

	return true;
}

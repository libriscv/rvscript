#pragma once

namespace api
{

struct vec2 {
	float x;
	float y;

	float length() const;
	vec2  rotate(float angle) const;
	vec2  normalized() const;
	void  normalize();
};

}

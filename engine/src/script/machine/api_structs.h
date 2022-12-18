#pragma once

namespace api
{

	struct vec2
	{
		float x;
		float y;

		float length() const;
		vec2 rotate(float angle) const;
		vec2 normalized() const;
		void normalize();
	};

	/** Startup function and arguments **/
	struct MapFile
	{
#ifdef __riscv
		const char* path;
		const char* file;
		const char* event;
#else
		gaddr_t path;
		gaddr_t file;
		gaddr_t event;
#endif
		int width;
		int height;
	};

}

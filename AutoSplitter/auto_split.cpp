
#include <obs-module.h>




// #####################################################

OBS_DECLARE_MODULE()

bool obs_module_load(void)
{
	return true;
}

void obs_module_unload(void)
{

	return;
}


const char* obs_module_author(void)
{
	return "FODK";
}

const char* obs_module_name(void)
{
	return "AutoSplit";
}

const char* obs_module_description(void)
{
	return "Stop/Start when the time coming";
}

#define BOOST_ALL_STATIC_LINK
#include <iostream>
#include <fstream>
#include <chrono>
#include <thread>
#include <boost/filesystem.hpp>
#include <boost/format.hpp>
#include <obs-module.h>

using namespace std;

#ifdef _DEBUG
#define OBLOG(...) blog(__VA_ARGS__)
#else
#define OBLOG(...) __noop
#endif

thread plugin_main_thread;
const chrono::milliseconds kWatchSpan = chrono::milliseconds(200);
const string kSettingFilename = "/settings.txt";
const vector<string> kOutputTypeTable{ "simple_file_output","adv_file_output","adv_ffmpeg_output" };

void LoadSettings(chrono::milliseconds &split_span,bool enabled_stop,bool enabled_restart)
{
	
	auto dir = obs_module_config_path(NULL);
	auto filename = obs_module_config_path(kSettingFilename.c_str());
	if (!boost::filesystem::exists(dir))
		boost::filesystem::create_directories(boost::filesystem::path(dir));
	if (!boost::filesystem::exists(filename))
	{
		ofstream ofs(filename);
		ofs << true << endl; // enabled_stop
		ofs << true << endl; // restart
		ofs << 0 << endl; // hour
		ofs << 10 << endl; // min
		ofs << 0 << endl; //sec
		ofs.close();
	}

	int hour, min, sec;
	ifstream ifs(filename);
	ifs >> enabled_stop;
	ifs >> enabled_restart;
	ifs >> hour;
	ifs >> min;
	ifs >> sec;
	ifs.close();
	split_span = chrono::seconds(sec) + chrono::minutes(min) + chrono::hours(hour);
}

bool getActiveOutput(obs_output_t* &output)
{
	for (string type_name : kOutputTypeTable)
	{
		output = obs_get_output_by_name(type_name.c_str());
		if (obs_output_active(output)) return true;
	}

	return false;
}

void ThreadEntryPoint()
{
	bool enabled_start = true;
	bool enabled_restart = true;
	chrono::milliseconds split_span;
	LoadSettings(split_span, enabled_start, enabled_restart);

	OBLOG(LOG_INFO,(boost::format("ASP: split span: %1%") % split_span.count()).str().c_str());
	OBLOG(LOG_INFO,(boost::format("ASP: enabled start: %1%") % enabled_start).str().c_str());

	if (!enabled_start) return;
	auto started_time = chrono::system_clock::now();
	while (true)
	{
		obs_output_t* output;
		bool is_actived = getActiveOutput(output);
		if (is_actived)
			started_time = chrono::system_clock::now();

		while(is_actived && ((chrono::system_clock::now() - started_time) < split_span))
		{
			this_thread::sleep_for(kWatchSpan);
			is_actived = obs_output_active(output);
		}

		if (is_actived)
		{
			obs_output_stop(output);
			if (enabled_restart)
				obs_output_start(output);
		}
	}

}


// #####################################################

OBS_DECLARE_MODULE()

bool obs_module_load(void)
{
	plugin_main_thread = thread(ThreadEntryPoint);
	return true;
}

void obs_module_unload(void)
{
	plugin_main_thread.detach();
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

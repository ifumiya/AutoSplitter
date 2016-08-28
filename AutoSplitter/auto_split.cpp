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
#define DLOG(...) blog(LOG_INFO,(string("AutoSplitter: ") + string(__VA_ARGS__)).c_str())
#define FLOG(...) blog(LOG_INFO, (string("AutoSplitter: ") + (boost::format __VA_ARGS__).str()).c_str())
#else
#define FLOG(...) __noop
#define DLOG(...) __noop
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
		ofs << 0 << endl;    // hour
		ofs << 10 << endl;   // min
		ofs << 0 << endl;    // sec
		
		ofs << "Enable(1) / Disable(0) this plugin" << endl;
		ofs << "Enable(1) / Disable(0) restart recording" << endl;
		ofs << "recording time (hours)" << endl;
		ofs << "recording time (mins)" << endl;
		ofs << "recording time (secs)" << endl;

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
	DLOG("Watch thread is launched");
	bool enabled_start = true;
	bool enabled_restart = true;
	chrono::milliseconds split_span;
	LoadSettings(split_span, enabled_start, enabled_restart);

	FLOG(("span: %1% / act %2% / rst %3%") % split_span.count() % enabled_start % enabled_restart);

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
		this_thread::sleep_for(kWatchSpan);
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

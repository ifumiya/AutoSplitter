#define BOOST_ALL_STATIC_LINK
#include <iostream>
#include <fstream>
#include <chrono>
#include <thread>

#include <boost/filesystem.hpp>
#include <boost/format.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/ini_parser.hpp>

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
const string kSettingFilename = "/settings.ini";
const vector<string> kOutputTypeTable{ "simple_file_output","adv_file_output","adv_ffmpeg_output" };



void LoadSettings(chrono::milliseconds &split_span,bool &enabled_stop,bool &enabled_restart)
{
	auto dir = obs_module_config_path(NULL);
	auto filename = obs_module_config_path(kSettingFilename.c_str());
	if (!boost::filesystem::exists(dir))
		boost::filesystem::create_directories(boost::filesystem::path(dir));
	if (!boost::filesystem::exists(filename))
	{
		boost::property_tree::ptree default_conf;
		default_conf.put("feature.enable_stop", true);
		default_conf.put("feature.enable_restart", true);
		default_conf.put("split_timespan.hours", 0);
		default_conf.put("split_timespan.minutes", 10);
		default_conf.put("split_timespan.seconds", 0);

		boost::property_tree::write_ini(filename, default_conf);
	}
	
	boost::property_tree::ptree conf;
	boost::property_tree::read_ini(filename, conf);

	int hour, min, sec;
	enabled_stop = conf.get<bool>("feature.enable_stop");
	enabled_restart = conf.get<bool>("feature.enable_stop");
	hour = conf.get<int>("split_timespan.hours");
	min  = conf.get<int>("split_timespan.minutes");
	sec  = conf.get<int>("split_timespan.seconds");

	split_span = chrono::seconds(sec) + chrono::minutes(min) + chrono::hours(hour);

	FLOG(
		("LoadSettings [enable_stop = %1% , enable_restart = %2% , stime_timespan h/m/s = %3%/%4%/%5% total = %6%]")
		% enabled_stop % enabled_restart
		% hour % min % sec % split_span.count()
	);
}

obs_output_t* getActiveOutput()
{
	obs_output_t* output;
	for (string type_name : kOutputTypeTable)
	{
		output = obs_get_output_by_name(type_name.c_str());
		if (obs_output_active(output))return output;
	}
	return nullptr;
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
	
	string base_filename;
	bool prev_active = false;
	int split_count = 0;
	while (true)
	{
		obs_output_t* output = getActiveOutput();
		bool is_active = obs_output_active(output);
		if (is_active && !prev_active)
		{
			// new record
			started_time = chrono::system_clock::now();
			split_count = 0;
			auto out_settings = obs_output_get_settings(output);
			base_filename = string(obs_data_get_string(out_settings, "path"));
			FLOG(("set base filename (%1%)") % base_filename);
			obs_data_release(out_settings);
		}

		while (is_active && ((chrono::system_clock::now() - started_time) < split_span))
		{
			// wait for split_span || wait until stop recording
			this_thread::sleep_for(kWatchSpan);
			is_active = obs_output_active(output);
		}

		if (is_active)
		{
			obs_output_stop(output);

			if (enabled_restart)
			{
				split_count++;
				auto split_count_str = (boost::format("%04d") % split_count).str();
				auto new_path = boost::filesystem::path(base_filename);
				new_path.replace_extension("_" + split_count_str + new_path.extension().string());
				auto out_settings = obs_output_get_settings(output);
				FLOG(("new filename (%1%)") % base_filename);

				obs_data_set_string(out_settings, "path", (new_path.string().c_str()));
				obs_output_update(output, out_settings);
				obs_data_release(out_settings);

				obs_output_start(output);
				started_time = chrono::system_clock::now();
			}
		}
		prev_active = is_active;
		obs_output_release(output);
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

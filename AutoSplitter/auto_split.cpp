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

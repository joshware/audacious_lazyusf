#ifndef _USF_PLUGIN_H_
#define _USF_PLUGIN_H_

/* For standalone compiling */
#ifndef PACKAGE
#define PACKAGE "audacious-plugins"
#endif

#ifndef EXPORT
#define EXPORT __attribute__((visibility("default")))
#endif

//AUD_PLUGIN_VERSION

#include <usf/usf.h>
#include "psftag.h"

class LazyUSF2Plugin : public InputPlugin
{
public:
    static const char about[];
    static const char * const exts[];
    static const char * const defaults[];
    static const PreferencesWidget widgets[];
    static const PluginPreferences prefs;

	static constexpr PluginInfo info = {
		N_("LazyUSF2 USF Decoder"),
		PACKAGE,
        about,
#ifdef HAS_PREFS
        &prefs
#endif
	};

    static constexpr auto iinfo = InputInfo ()
        .with_exts (exts);

    constexpr LazyUSF2Plugin() : InputPlugin (info, iinfo), state(nullptr), play_length(300.0), fade_length(0.0) {}

    bool init ();
    void cleanup ();

    bool is_our_file (const char * filename, VFSFile & file);
    Tuple read_tuple (const char * filename, VFSFile & file);
    bool write_tuple (const char * filename, VFSFile & file, const Tuple & tuple);
    bool play (const char * filename, VFSFile & file);
    void update(const void *data, int bytes);

private:
	usf_state_t * state;
	bool usf_load(const char * file_name, VFSFile & file);
	double play_length;
	double fade_length;
};



#endif

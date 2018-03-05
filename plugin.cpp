#define WANT_VFS_STDIO_COMPAT

#include <stdlib.h>
#include <string.h>

#include <libaudcore/plugin.h>
#include <libaudcore/i18n.h>
#include <libaudcore/audstrings.h>
#include <libaudcore/runtime.h>
#include <libaudcore/preferences.h>
#include <audacious/audtag.h>


#include "plugin.h"
const int32_t sample_rate = 44100;
const int32_t USF_CHANNELS = 2;
const int32_t USF_BITS_PER_SAMPLE = 16;

EXPORT LazyUSF2Plugin aud_plugin_instance;

const char * const LazyUSF2Plugin::defaults[] = {
    "use_hle", "TRUE",
    "use_interpreter", "FALSE",
    "infinite_play", "FALSE",
    nullptr
};

const char LazyUSF2Plugin::about[] = N_(
    "LazyUSF2 USF Decoder for Audacious\n\n"
    "Plugin by Josh W\n\n"
    "liblazyusf2 by Kode54 \nhttps://gitlab.kode54.net/kode54/lazyusf2\n\n"
);

bool LazyUSF2Plugin::init()
{
    aud_config_set_defaults ("lazyusf2", defaults);
    state = nullptr;

    return true;
}

void LazyUSF2Plugin::cleanup()
{
    if(state != nullptr)
    {
        usf_shutdown(state);
        free(state);
        state = nullptr;
    }
}

bool LazyUSF2Plugin::is_our_file (const char * fname, VFSFile & file)
{
    if(strstr(fname, ".usf") ||
       strstr(fname, ".miniusf"))
    {
        char buffer[4];

        if( file.fseek(0, VFS_SEEK_SET) != 0 )
        {
            return false;
        }

        if( file.fread(buffer, 4, 1) == 0 )
        {
            return false;
        }

        if( buffer[0] != 'P' && buffer[1] != 'S' && buffer[2] != 'F' && buffer[3] != 0x21 )
        {
            return false;
        }

        return true;
    }

    return false;
}

static int32_t parse_time(char * length)
{
    uint32_t ms = 0, count = 0;
    double entries[3], out_length;

    if( !strlen(length) || !*length )
    {
        return 0;
    }

    count = sscanf(length, "%lf:%lf:%lf", &entries[0], &entries[1], &entries[2]);

    out_length = entries[0];
    if( count > 1 )
    {
        out_length = (out_length * 60) + entries[1];
    }
    if( count > 2 )
    {
        out_length = (out_length * 60) + entries[2];
    }
    return (int32_t) (out_length * 1000.0);
}

Tuple LazyUSF2Plugin::read_tuple(const char *filename, VFSFile & file)
{
    Tuple t;
    char buffer[4];
    int32_t reservedsize, codesize, crc, tagsize, tagstart, reservestart, filesize;

    t.set_filename (filename);

    if( file.fseek(0, VFS_SEEK_SET) != 0 )
    {
        return t;
    }

    if( file.fread(buffer, 4, 1) == 0 )
    {
        return t;
    }

    if( buffer[0] != 'P' && buffer[1] != 'S' && buffer[2] != 'F' && buffer[3] != 0x21 )
    {
        return t;
    }

    if( file.fread(&reservedsize, 4, 1) == 0 )
    {
        return t;
    }

    if( file.fread(&codesize, 4, 1) == 0 )
    {
        return t;
    }

    if( file.fread(&crc, 4, 1) == 0 )
    {
        return t;
    }

    filesize = file.fsize();

    reservestart = 0x10;
    tagstart = reservestart + reservedsize;
    tagsize = filesize - tagstart - strlen("[TAG]");

    if(tagsize > 0)
    {
        char *buffer, *tagbuffer;
        int32_t length = 300, fade = 0;

        buffer = new char[50001];
        tagbuffer = new char[tagsize];

        if( file.fseek(tagstart, VFS_SEEK_SET) != 0 )
        {
             return t;
        }

        if( file.fread(buffer, 5, 1) == 0)
        {
            return t;
        }

        if( strncmp(buffer, "[TAG]", 5) )
        {
            return t;
        }

        if( file.fread(tagbuffer, tagsize, 1) == 0 )
        {
            return t;
        }

        psftag_raw_getvar(tagbuffer, "length", buffer, 50000);
        length = parse_time(buffer);
        psftag_raw_getvar(tagbuffer, "fade", buffer, 50000);
        fade = parse_time(buffer);
        if( length > 0 )
        {
            t.set_int(Tuple::Length, length + fade);
        }
        else
        {
            t.set_int(Tuple::Length, 300 * 1000);
        }

        psftag_raw_getvar(tagbuffer, "artist", buffer, 50000);
        if( strlen( buffer ) )
        {
            t.set_str(Tuple::Artist, buffer);
        }

        psftag_raw_getvar(tagbuffer, "game", buffer, 50000);
        if( strlen( buffer ) )
        {
            t.set_str(Tuple::Album, buffer);
        }

        psftag_raw_getvar(tagbuffer, "title", buffer, 50000);
        if( strlen( buffer ) )
        {
            t.set_str(Tuple::Title, buffer);
        }

        psftag_raw_getvar(tagbuffer, "copyright", buffer, 50000);
        t.set_str(Tuple::Copyright, buffer);

        delete buffer;
        delete tagbuffer;
    }
    else
    {
        t.set_int(Tuple::Length, 300 * 1000);
    }

    t.set_str(Tuple::Quality, _("sequenced"));
    t.set_str (Tuple::Codec, "Nintendo 64 Audio");

    return t;
}

static void fade_audio(int16_t *block, int32_t samples, double time, double fade_time)
{
    double begin_amount = 1.0f - (time / fade_time);
    double amount_per_sample = 0.5f / (fade_time * (double)sample_rate);
    uint32_t i;

    for( i = 0; i < (samples << 1); i++ )
    {
        double fade_amount = begin_amount - (i * amount_per_sample);
        *block = (int16_t)((double)*block * fade_amount);
        block++;
    }
}

bool LazyUSF2Plugin::usf_load(const char * file_name, VFSFile & file) {
    int32_t reservedsize = 0, codesize = 0, crc = 0, tagstart = 0, reservestart = 0, filesize = 0, tagsize = 0, temp = 0;
    int32_t length = 300000, fade = 5000;
    char buffer[16], * buffer2 = NULL, * tagbuffer = NULL;
    const uint8_t * section;

    if( file.fseek(0, VFS_SEEK_SET) != 0 )
    {
        return false;
    }

    if( file.fread(buffer, 4, 1) == 0 )
    {
        return false;
    }

    if( buffer[0] != 'P' && buffer[1] != 'S' && buffer[2] != 'F' && buffer[3] != 0x21 )
    {
        return false;
    }

    if( file.fread(&reservedsize, 4, 1) == 0 )
    {
        return false;
    }

    if( file.fread(&codesize, 4, 1) == 0 )
    {
        return false;
    }

    if( file.fread(&crc, 4, 1) == 0 )
    {
        return false;
    }

    filesize = file.fsize();
    reservestart = 0x10;
    tagstart = reservestart + reservedsize;
    tagsize = filesize - tagstart - strlen("[TAG]");

    if(tagsize > 0)
    {
        if( file.fseek(tagstart, VFS_SEEK_SET) != 0 )
        {
             return false;
        }

        if( file.fread(buffer, 5, 1) == 0)
        {
            return false;
        }

        if( strncmp(buffer, "[TAG]", 5) )
        {
            return false;
        }

        buffer2 = new char[50001];
        tagbuffer = new char[tagsize];

        if( file.fread(tagbuffer, tagsize, 1) == 0 )
        {
            delete [] buffer2;
            delete [] tagbuffer;
            return false;
        }

        psftag_raw_getvar(tagbuffer, "_lib", buffer2, 50000);

        if(strlen(buffer2))
        {
            char path[512];
            int pathlength = 0;

            if( strrchr(file_name, '/') )
            {
                pathlength = strrchr(file_name, '/') - file_name + 1;
            }
            else if( strrchr(file_name, '\\') )
            {
                pathlength = strrchr(file_name, '\\') - file_name + 1;
            }
            else //no path
            {
                pathlength = strlen(file_name);
            }

            strncpy(path, file_name, pathlength);
            path[pathlength] = 0;
            strcat(path, buffer2);

            VFSFile vfsFile(path, "rb");

            if( usf_load(path, vfsFile) == false )
            {
                delete [] buffer2;
                delete [] tagbuffer;
                return false;
            }
        }

        psftag_raw_getvar(tagbuffer,"_enablecompare",buffer2,50000);
        if( strlen(buffer2) > 0 )
        {
            usf_set_compare(state, 1);
        }

        psftag_raw_getvar(tagbuffer,"_enableFIFOfull",buffer2,50000);
        if( strlen(buffer2) > 0 )
        {
            usf_set_fifo_full(state, 1);
        }

        psftag_raw_getvar(tagbuffer, "length", buffer, 50000);
        if( strlen(buffer) )
        {
            length = parse_time(buffer);
        }

        psftag_raw_getvar(tagbuffer, "fade", buffer, 50000);
        if( strlen(buffer) )
        {
            /* Adding a bit more fade, to prevent it from going to the next track too soon */
            fade = parse_time(buffer) + 1000;
        }

        delete [] buffer2;
        delete [] tagbuffer;
    }

    fade_length = fade / 1000.0f;
    play_length = (length + fade) / 1000.0f;

    if( file.fseek(reservestart, VFS_SEEK_SET) != 0 )
    {
        return false;
    }

    section = new uint8_t[reservedsize];

    if( file.fread((void *)section, reservedsize, 1) == 0 )
    {
        delete [] section;
        return false;
    }

    if( usf_upload_section(state, section, reservedsize) == -1 )
    {
        delete [] section;
        return false;
    }

    delete section;
    return true;
}

bool LazyUSF2Plugin::play (const char * filename, VFSFile & file)
{
    double play_position = 0.0, seek_position = 0.0;
    static size_t frame_size = 4000;
    bool is_seeking = false;

    if(state != nullptr)
    {
        usf_shutdown(state);
        free(state);
    }

    state = (usf_state_t *)malloc(usf_get_state_size());
    usf_clear(state);

    if( usf_load(filename, file) == 0 )
    {
        usf_shutdown(state);
        free(state);
        state = nullptr;
        return false;
    }

    set_stream_bitrate(sample_rate * USF_CHANNELS * USF_BITS_PER_SAMPLE);
    open_audio(FMT_S16_NE, sample_rate, 2);

    while( !check_stop() && play_position <= play_length )
    {
        int16_t frame[frame_size << 1];
        int32_t seek_check = check_seek();
        double seek_pos = (double)seek_check / 1000.0f;
        const char * e;
        
        /* seek_check will be not -1, and seek_pos will be in seconds */
        if( seek_check >= 0 && is_seeking == false )
        {
            is_seeking = true;
            seek_position = seek_pos;

            if( seek_pos < play_position)
            {            
                play_position = 0.0;
                usf_restart(state);
            }
        }
        else if( is_seeking == true && play_position >= seek_position )
        {
            is_seeking = false;
        }

        e = usf_render_resampled(state, frame, frame_size, sample_rate);
        if( e != NULL )
        {
            usf_shutdown(state);
            free(state);
            state = nullptr;
            return false;
        }

        play_position += ( (double)frame_size / (double)sample_rate );

        if( is_seeking == false )
        {
            if( fade_length && play_position > (play_length - fade_length) )
            {
                fade_audio(frame, frame_size, play_position - (play_length - fade_length), fade_length);
            }
            write_audio((char *)frame, frame_size << 2);
        }
    }

    usf_shutdown(state);
    free(state);
    state = nullptr;
    return true;
}


bool LazyUSF2Plugin::write_tuple (const char * filename, VFSFile & file, const Tuple & tuple)
{
    return false;
}

const char *const LazyUSF2Plugin::exts[] = { "usf", "miniusf", nullptr };




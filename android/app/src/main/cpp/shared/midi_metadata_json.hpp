#ifndef DXX_MIDI_METADATA_JSON_HPP
#define DXX_MIDI_METADATA_JSON_HPP

#include <algorithm>
#include <cctype>
#include <string>

extern "C" {
#include "midi_metadata_physfs.h"
#include "songs.h"

extern bim_song_info *BIMSongs;
extern int Num_bim_songs;
void songs_init(void);
}

static inline std::string midi_metadata_lower_extension(const char *filename)
{
	const std::string value = filename ? filename : "";
	const std::string::size_type dot = value.find_last_of('.');
	if (dot == std::string::npos)
		return {};
	std::string extension = value.substr(dot + 1);
	std::transform(extension.begin(), extension.end(), extension.begin(),
	               [](unsigned char value) { return static_cast<char>(std::tolower(value)); });
	return extension;
}

static inline const char *midi_metadata_slot_kind(int index)
{
	switch (index) {
		case SONG_TITLE: return "title";
		case SONG_BRIEFING: return "briefing";
		case SONG_ENDLEVEL: return "endlevel";
		case SONG_ENDGAME: return "endgame";
		case SONG_CREDITS: return "credits";
		default: return "level";
	}
}

template <typename Json>
static Json serialize_active_midi_metadata()
{
	Json tracks = Json::array();
	songs_init();
	for (int index = 0; BIMSongs && index < Num_bim_songs; ++index) {
		const char *filename = BIMSongs[index].filename;
		const std::string format = midi_metadata_lower_extension(filename);
		midi_metadata metadata;
		char source_filename[MIDI_METADATA_SOURCE_FILENAME_BYTES] = "";
		int inherited = 0;
		Json row;
		Json events = Json::array();
		if (format != "mid" && format != "midi" && format != "hmp" && format != "hmq")
			continue;
		midi_metadata_init(&metadata);
		midi_metadata_resolve_physfs(filename, &metadata, source_filename,
		                             sizeof(source_filename), &inherited);
		row["slot_index"] = index;
		row["slot_kind"] = midi_metadata_slot_kind(index);
		row["filename"] = filename;
		row["format"] = format;
		row["metadata_source_filename"] = source_filename;
		row["inherited_from_midi"] = inherited != 0;
		row["parse_status"] = midi_metadata_status_name(metadata.status);
		row["smf_format"] = metadata.smf_format;
		row["track_count"] = metadata.track_count;
		row["time_division"] = metadata.time_division;
		row["title"] = metadata.title;
		row["composer"] = metadata.composer;
		row["display_name"] = metadata.display_name;
		row["metadata_truncated"] = metadata.metadata_truncated != 0;
		for (unsigned int event_index = 0; event_index < metadata.event_count; ++event_index) {
			const midi_metadata_text_event &event = metadata.events[event_index];
			events.push_back({
				{"track_index", event.track_index},
				{"type", midi_metadata_event_type_name(event.type)},
				{"text", event.text ? event.text : ""},
			});
		}
		row["text_events"] = events;
		tracks.push_back(row);
		midi_metadata_free(&metadata);
	}
	return tracks;
}

#endif

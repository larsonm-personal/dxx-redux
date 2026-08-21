#ifndef DXX_MIDI_METADATA_JSON_HPP
#define DXX_MIDI_METADATA_JSON_HPP

#include <algorithm>
#include <cctype>
#include <string>

#include "audio_tag_metadata.h"

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
static Json serialize_active_music_metadata()
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
		const bool is_midi = format == "mid" || format == "midi" || format == "hmp" || format == "hmq";
		const bool is_audio = format == "mp3" || format == "ogg" || format == "flac";
		if (!is_midi && !is_audio) continue;
		if (is_audio) {
			audio_tag_metadata audio_metadata;
			Json properties = Json::array();
			audio_tag_metadata_init(&audio_metadata);
			audio_tag_metadata_parse_physfs(filename, &audio_metadata);
			row["slot_index"] = index;
			row["slot_kind"] = midi_metadata_slot_kind(index);
			row["filename"] = filename;
			row["format"] = format;
			row["parse_status"] = audio_tag_status_name(audio_metadata.status);
			row["title"] = audio_metadata.title;
			row["composer"] = audio_metadata.composer;
			row["artist"] = audio_metadata.artist;
			row["album_artist"] = audio_metadata.album_artist;
			row["album"] = audio_metadata.album;
			row["date"] = audio_metadata.date;
			row["genre"] = audio_metadata.genre;
			row["comment"] = audio_metadata.comment;
			row["copyright"] = audio_metadata.copyright;
			row["track_number"] = audio_metadata.track_number;
			row["disc_number"] = audio_metadata.disc_number;
			row["display_name"] = audio_metadata.display_name;
			row["metadata_truncated"] = audio_metadata.metadata_truncated != 0;
			for (unsigned int property_index = 0; property_index < audio_metadata.property_count; ++property_index) {
				const audio_tag_property &property = audio_metadata.properties[property_index];
				Json values = Json::array();
				for (unsigned int value_index = 0; value_index < property.value_count; ++value_index)
					values.push_back(property.values[value_index] ? property.values[value_index] : "");
				properties.push_back({ { "key", property.key ? property.key : "" }, { "values", values } });
			}
			row["properties"] = properties;
			tracks.push_back(row);
			audio_tag_metadata_free(&audio_metadata);
			continue;
		}
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

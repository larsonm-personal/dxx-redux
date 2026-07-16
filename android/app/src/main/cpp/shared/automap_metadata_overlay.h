#ifndef AUTOMAP_METADATA_OVERLAY_H
#define AUTOMAP_METADATA_OVERLAY_H

void automap_metadata_draw_labels(
    int *secret_candidate_count,
    int *secret_projected_count,
    int *objective_visible_step_count,
    int *objective_candidate_count,
    int *objective_projected_count);
void automap_metadata_draw_connectors(
    int *objective_candidate_count,
    int *objective_drawn_count);
void automap_metadata_draw_next_objectives(int *objective_count);
int automap_metadata_get_key_carrier_marker(
    int *objnum, int *key_index, int position[3]);
int automap_metadata_get_key_carrier_marker_count(void);
int automap_metadata_get_merged_objective_label_count(void);
const char *automap_metadata_get_first_merged_objective_label(void);

int secret_area_should_draw_segment_edges(int segnum);
int automap_segment_is_within_limit(int segnum, int segment_limit);

#endif /* AUTOMAP_METADATA_OVERLAY_H */

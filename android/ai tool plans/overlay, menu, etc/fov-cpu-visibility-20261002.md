# CPU-only baseline visibility for custom FOV

Status: implementation in progress

The main-view FOV override currently renders a complete baseline view before the visual view. Replace the baseline draw with CPU projection, original portal traversal and object ordering, homing-list publication, automap discovery and demo records. Preserve base FOV, subviews and endlevel behavior. Keep shared logic in the existing render_gameplay_view service, with guarded D1/D2 integration

Audit: g3_start_frame_projection and render_setup_view already provide CPU-only setup. build_segment_list/build_object_lists are the existing CPU traversal services. do_render_object owns homing-list ordering and D2 demo-view exclusions. render_object emits object demo records, including an earlier morph-frame record in draw_morph_object. The CPU pass must retain those records and the visual pass must suppress duplicates. Lighting and model/sprite/texture preparation belong to the single visual pass

Validation: add opt-in comparison against the original baseline draw, checking ordered segments, homing candidates and automap discovery on the same frame. Verify one actual main scene pass with the existing MSAA pixel probe, across FOV values and depth modes, then rear/missile subview composition. Run serial D1/D2 integration on an available emulator, Android builds and both Windows builds. Preserve concurrent workspace changes and personal device data

The separately reported resolution/context GPU query error is outside this optimization

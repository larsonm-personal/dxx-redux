/* D1 POF animation grouping, with caller-owned joint storage */
#ifndef DXX_D1_SHAREWARE_JOINTS_H
#define DXX_D1_SHAREWARE_JOINTS_H

static inline int d1_shareware_build_joints(robot_info *robot, const polymodel *model,
                                            const vms_angvec angles[N_ANIM_STATES][MAX_SUBMODELS], jointpos *joints, int *count, int capacity)
{
	int groups[MAX_SUBMODELS], submodel, gun, state;
	if (model->n_models < 1 || model->n_models > MAX_SUBMODELS ||
	    robot->n_guns < 0 || robot->n_guns > MAX_GUNS || *count < 0 || *count > capacity)
		return 0;
	for (submodel = 0; submodel < model->n_models; ++submodel) groups[submodel] = robot->n_guns;
	groups[0] = -1;
	for (gun = 0; gun < robot->n_guns; ++gun) {
		int depth = 0;
		submodel = robot->gun_submodels[gun];
		while (submodel != 0) {
			if (submodel >= model->n_models || ++depth >= model->n_models) return 0;
			groups[submodel] = gun;
			submodel = model->submodel_parents[submodel];
		}
	}
	if (model->n_models - 1 > (capacity - *count) / N_ANIM_STATES) return 0;
	for (gun = 0; gun <= robot->n_guns; ++gun)
		for (state = 0; state < N_ANIM_STATES; ++state) {
			jointlist *list = &robot->anim_states[gun][state];
			list->n_joints = 0;
			list->offset = *count;
			for (submodel = 0; submodel < model->n_models; ++submodel)
				if (groups[submodel] == gun) {
					joints[*count].jointnum = submodel;
					joints[*count].angles = angles[state][submodel];
					++list->n_joints;
					++*count;
				}
		}
	return 1;
}
#endif

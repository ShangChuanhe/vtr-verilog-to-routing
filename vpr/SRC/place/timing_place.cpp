#include <cstdio>
#include <cmath>
using namespace std;

#include "vtr_util.h"
#include "vtr_memory.h"
#include "vtr_log.h"

#include "vpr_types.h"
#include "vpr_utils.h"
#include "globals.h"
#include "path_delay.h"
#include "path_delay2.h"
#include "net_delay.h"
#include "timing_place_lookup.h"
#include "timing_place.h"

#include "timing_info.h"

static vtr::vector_map<ClusterNetId, float *> f_timing_place_crit; /* [0..cluster_ctx.clb_nlist.nets().size()-1][1..num_pins-1] */

static vtr::t_chunk f_timing_place_crit_ch = {NULL, 0, NULL};

/******** prototypes ******************/
static void alloc_crit(vtr::t_chunk *chunk_list_ptr);

static void free_crit(vtr::t_chunk *chunk_list_ptr);

/**************************************/

/* Allocates space for the f_timing_place_crit data structure *
* I chunk the data to save space on large problems.           */
static void alloc_crit(vtr::t_chunk *chunk_list_ptr) {
	auto& cluster_ctx = g_vpr_ctx.clustering();
	float *tmp_ptr;

	f_timing_place_crit.resize(cluster_ctx.clb_nlist.nets().size());

	for (auto net_id : cluster_ctx.clb_nlist.nets()) {
		tmp_ptr = (float *) vtr::chunk_malloc(
				(cluster_ctx.clb_nlist.net_sinks(net_id).size()) * sizeof(float), chunk_list_ptr);
		f_timing_place_crit[net_id] = tmp_ptr - 1; /* [1..num_sinks] */
	}
}

/**************************************/
static void free_crit(vtr::t_chunk *chunk_list_ptr){
    vtr::free_chunk_memory(chunk_list_ptr);
}

/**************************************/
void load_criticalities(SetupTimingInfo& timing_info, float crit_exponent, const IntraLbPbPinLookup& pb_gpin_lookup) {
	/* Performs a 1-to-1 mapping from criticality to f_timing_place_crit.  
	  For every pin on every net (or, equivalently, for every tedge ending 
	  in that pin), f_timing_place_crit = criticality^(criticality exponent) */

    auto& cluster_ctx = g_vpr_ctx.clustering();
	for (auto net_id : cluster_ctx.clb_nlist.nets()) {
		if (cluster_ctx.clb_nlist.net_is_global(net_id))
			continue;
		for (size_t ipin = 1; ipin < cluster_ctx.clb_nlist.net_pins(net_id).size(); ipin++) {
            float clb_pin_crit = calculate_clb_net_pin_criticality(timing_info, pb_gpin_lookup, net_id, ipin);

            /* The placer likes a great deal of contrast between criticalities. 
            Since path criticality varies much more than timing, we "sharpen" timing 
            criticality by taking it to some power, crit_exponent (between 1 and 8 by default). */
            f_timing_place_crit[net_id][ipin] = pow(clb_pin_crit, crit_exponent);
		}
	}
}


float get_timing_place_crit(ClusterNetId net_id, int ipin) {
    return f_timing_place_crit[net_id][ipin];
}

void set_timing_place_crit(ClusterNetId net_id, int ipin, float val) {
    f_timing_place_crit[net_id][ipin] = val;
}

/**************************************/
void alloc_lookups_and_criticalities(t_chan_width_dist chan_width_dist,
		t_router_opts router_opts,
		t_det_routing_arch *det_routing_arch, t_segment_inf * segment_inf,
		const t_direct_inf *directs, 
		const int num_directs) {

	compute_delay_lookup_tables(router_opts, det_routing_arch, segment_inf,
			chan_width_dist, directs, num_directs);
	
	alloc_crit(&f_timing_place_crit_ch);
}

/**************************************/
void free_lookups_and_criticalities() {
	//TODO: May need to free f_timing_place_crit ?
	free_crit(&f_timing_place_crit_ch);

	free_place_lookup_structs();
}

/**************************************/

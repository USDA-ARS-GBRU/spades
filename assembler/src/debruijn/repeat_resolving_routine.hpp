/*
 * repeat_resolving_routine.hpp
 *
 *  Created on: 1 Sep 2011
 *      Author: valery
 */

#pragma once

#include "standard.hpp"

#include "logging.hpp"
#include "repeat_resolving.hpp"
#include "distance_estimation_routine.hpp"
typedef io::Reader<io::SingleRead> ReadStream;

namespace debruijn_graph

{
void resolve_repeats(PairedReadStream& stream, const Sequence& genome);
} // debruijn_graph


// move impl to *.cpp

namespace debruijn_graph
{

void FillContigNumbers(   map<ConjugateDeBruijnGraph::EdgeId, int>& contigNumbers,  ConjugateDeBruijnGraph& cur_graph){
	int cur_num = 0;
	set<ConjugateDeBruijnGraph::EdgeId> edges;
	for (auto iter = cur_graph.SmartEdgeBegin(); !iter.IsEnd(); ++iter) {
		if (edges.find(*iter) == edges.end()) {
			contigNumbers[*iter] = cur_num;
			cur_num++;
			edges.insert(cur_graph.conjugate(*iter));
		}
	}
}

void FillContigNumbers(   map<NonconjugateDeBruijnGraph::EdgeId, int>& contigNumbers,  NonconjugateDeBruijnGraph& cur_graph){
	int cur_num = 0;
	for (auto iter = cur_graph.SmartEdgeBegin(); !iter.IsEnd(); ++iter) {
		contigNumbers[*iter] = cur_num;
	    cur_num++;
	}
}

template<size_t k, class Graph>
void SelectReadsForConsensus(Graph& etalon_graph, Graph& cur_graph,
        EdgeLabelHandler<Graph>& LabelsAfter,
        const EdgeIndex<K + 1, Graph>& index ,vector<ReadStream *>& reads
        , string& consensus_output_dir)
{
    INFO("ReadMapping started");
    map<typename Graph::EdgeId, int> contigNumbers;
    int cur_num = 0;
    FillContigNumbers(contigNumbers, cur_graph);
//    for (auto iter = cur_graph.SmartEdgeBegin(); !iter.IsEnd(); ++iter) {
//        contigNumbers[*iter] = cur_num;
//        cur_num++;
//    }
//
    cur_num = contigNumbers.size();
    INFO(cur_num << "contigs");
    for (int i = 1; i < 3; i++) {
        int read_num = 0;
        osequencestream* mapped_reads[4000];
        for (int j = 0; j < cur_num; j++) {
            string output_filename = consensus_output_dir + ToString(j)
                    + "_reads" + ToString(i) + ".fa";
            osequencestream* tmp = new osequencestream(output_filename);
//          mapped_reads.push_back(tmp);
            mapped_reads[j] = tmp;
        }
        SingleReadMapper<k, Graph> rm(etalon_graph, index);
        while (!reads[i - 1]->eof()) {
            io::SingleRead cur_read;
            (*reads[i - 1]) >> cur_read;
            vector<typename Graph::EdgeId> res = rm.GetContainingEdges(
                    cur_read);
            read_num++;
            TRACE(
                    read_num<< " mapped to"<< res.size() <<" contigs :, read"<< cur_read.sequence());
//          map_quantity += res.size();
            for (size_t ii = 0; ii < res.size(); ii++) {
                TRACE("conting number "<< contigNumbers[res[ii]]);
                set<typename Graph::EdgeId> images =
                        LabelsAfter.edge_inclusions[res[ii]];
                for (auto iter = images.begin(); iter != images.end(); ++iter)
                    (*mapped_reads[contigNumbers[*iter]])
                            << cur_read.sequence();
            }
        }
    }
}


template <class graph_pack>
void process_resolve_repeats(
		graph_pack& origin_gp,
		PairedInfoIndex<typename graph_pack::graph_t>& clustered_index,
		graph_pack& resolved_gp)
{
    EdgeLabelHandler<typename graph_pack::graph_t> labels_after(resolved_gp.g, origin_gp.g);
    DEBUG("New index size: "<< clustered_index.size());
    // todo: make printGraph const to its arguments

    // todo: possibly we don't need it
//    if (cfg::get().rectangle_mode)
//        RectangleResolve(clustered_index, origin_gp.g, cfg::get().output_root + "tmp/", cfg::get().output_dir);

    ResolveRepeats(origin_gp  .g, origin_gp  .int_ids, clustered_index, origin_gp  .edge_pos,
                   resolved_gp.g, resolved_gp.int_ids,                  resolved_gp.edge_pos,
                   cfg::get().output_dir + "resolve/", labels_after);

    INFO("Total labeler start");

    typedef TotalLabelerGraphStruct<typename graph_pack::graph_t> total_labeler_gs;
    typedef TotalLabeler           <typename graph_pack::graph_t> total_labeler;

    total_labeler_gs graph_struct_before(origin_gp  .g, &origin_gp  .int_ids, &origin_gp  .edge_pos, NULL);
    total_labeler_gs graph_struct_after (resolved_gp.g, &resolved_gp.int_ids, &resolved_gp.edge_pos, &labels_after);

    total_labeler tot_labeler_after(&graph_struct_after, &graph_struct_before);

    omnigraph::WriteSimple(resolved_gp.g, tot_labeler_after, cfg::get().output_dir + "3_resolved_graph.dot", "no_repeat_graph");

    INFO("Total labeler finished");

    INFO("---Clearing resolved graph---");

    for (int i = 0; i < 3; ++i)
    {
        ClipTipsForResolver(resolved_gp.g);
        RemoveBulges2      (resolved_gp.g);
        RemoveLowCoverageEdgesForResolver(resolved_gp.g);
    }

    INFO("---Cleared---");
    INFO("---Output Contigs---");

    OutputContigs(resolved_gp.g, cfg::get().output_dir + "contigs_before_enlarge.fasta");

    omnigraph::WriteSimple(resolved_gp.g, tot_labeler_after,
                         cfg::get().output_dir + "4_cleared_graph.dot", "no_repeat_graph");

    if (cfg::get().need_consensus)
    {
        string consensus_folder = cfg::get().output_dir + "consensus_after_resolve/";
    	OutputSingleFileContigs(resolved_gp.g, consensus_folder);
    	string input_dir = cfg::get().input_dir;
		string reads_filename1 = input_dir + cfg::get().ds.first;
		string reads_filename2 = input_dir + cfg::get().ds.second;

		string real_reads = cfg::get().uncorrected_reads;
		if (real_reads != "none") {
			reads_filename1 = input_dir + (real_reads + "_1");
			reads_filename2 = input_dir + (real_reads + "_2");
		}

		ReadStream reads_1(reads_filename1);
		ReadStream reads_2(reads_filename2);

		vector<ReadStream*> reads = {&reads_1, &reads_2};

    	SelectReadsForConsensus<K, typename graph_pack::graph_t>(origin_gp.g, resolved_gp.g, labels_after, origin_gp.index, reads, consensus_folder);
    }

	one_many_contigs_enlarger<typename graph_pack::graph_t> N50enlarger(resolved_gp.g, cfg::get().ds.IS);
    N50enlarger.Loops_resolve();

    omnigraph::WriteSimple(resolved_gp.g, tot_labeler_after,
                         cfg::get().output_dir + "5_unlooped_graph.dot", "no_repeat_graph");

    OutputContigs(resolved_gp.g, cfg::get().output_dir + "contigs_unlooped.fasta");

    N50enlarger.one_many_resolve_with_vertex_split();

    omnigraph::WriteSimple(resolved_gp.g, tot_labeler_after,
                         cfg::get().output_dir + "6_finished_graph.dot", "no_repeat_graph");

    OutputContigs(resolved_gp.g, cfg::get().output_dir + "contigs_final.fasta");



    OutputContigs(origin_gp.g, cfg::get().output_dir + "contigs_before_resolve.fasta");

//    if (number_of_components > 0) {
//        string output_comp = output_folder + "resolved_comp";
//        mkdir(output_comp.c_str(),
//                S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH | S_IWOTH);
//
//        for (int i = 1; i <= number_of_components; i++)
//            ResolveOneComponent(output_folder + "graph_components/",
//                    output_comp + "/", i, k);
//    }

}



void resolve_repeats(PairedReadStream& stream, const Sequence& genome)
{
    conj_graph_pack   conj_gp (genome);
    paired_info_index paired_index   (conj_gp.g/*, 5.*/);
    paired_info_index clustered_index(conj_gp.g);

    exec_distance_estimation(stream, conj_gp, paired_index, clustered_index);

    INFO("STAGE == Resolving Repeats");

    if (!cfg::get().paired_mode)
    {
        OutputContigs(conj_gp.g, cfg::get().output_dir + "contigs.fasta");
        return;
    }
    if (cfg::get().rr.symmetric_resolve) {
    	conj_graph_pack   resolved_gp (genome);
    	process_resolve_repeats(conj_gp, clustered_index, resolved_gp) ;
    } else {
    	nonconj_graph_pack origin_gp(conj_gp, clustered_index);
    	nonconj_graph_pack resolved_gp;
    	process_resolve_repeats(origin_gp, origin_gp.clustered_index, resolved_gp) ;

    }

}


void exec_repeat_resolving(PairedReadStream& stream, const Sequence& genome)
{
	if (cfg::get().entry_point <= ws_repeats_resolving)
    {
		resolve_repeats(stream, genome);
		//todo why nothing to save???
        // nothing to save yet
    }
    else
    {
    	INFO("Loading Repeat Resolving");
    	INFO("Nothing to load");
    	// nothing to load
    }
}

} // debruijn_graph







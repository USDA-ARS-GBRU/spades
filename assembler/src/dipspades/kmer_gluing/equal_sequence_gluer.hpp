#pragma once

#include "../utils/edge_gluer.hpp"

using namespace debruijn_graph;

namespace dipspades {

template<class Graph>
class EqualSequencesGluer {
private:
	typedef typename Graph::EdgeId EdgeId;
	typedef typename Graph::VertexId VertexId;

	Graph &graph_;
	conj_graph_pack::index_t &index_;

	EdgeId ExtractShortEdge(EdgeId edge, size_t pos) {
		if(pos + 1 < graph_.length(edge)) {
			edge = graph_.SplitEdge(edge, pos + 1).first;
		}
		if(pos > 0) {
			edge = graph_.SplitEdge(edge, pos).second;
		}
		VERIFY(graph_.length(edge) == 1);
		return edge;
	}

	void GlueEqualEdgeParts(EdgeId edge1, size_t pos1, EdgeId edge2, size_t pos2) {
		TRACE("Edge1: " << graph_.int_id(edge1) << ", length: " << graph_.length(edge1) << ", pos: " << pos1);
		TRACE("Edge2: " << graph_.int_id(edge2) << ", length: " << graph_.length(edge2) << ", pos: " << pos2);
		VERIFY(edge1 != edge2 || pos1 != pos2);
		if(edge1 == edge2) {
			if(edge1 == graph_.conjugate(edge2)) {
				WARN("Equal k-mer gluer faced a difficult situation in graph. Equal k-mers were ignored.");
				return;
			}
			if(pos1 > pos2) {
				size_t tmp = pos1;
				pos1 = pos2;
				pos2 = tmp;
			}
			pair<EdgeId, EdgeId> split_edges = graph_.SplitEdge(edge1, pos2);
			edge1 = split_edges.first;
			edge2 = split_edges.second;
			pos2 = 0;
		} else if(edge1 == graph_.conjugate(edge2)) {
			TRACE("Edges are conjugate pairs");
			if(graph_.length(edge1) - pos1 - pos2 <= 1 or pos1 == pos2) {
				WARN("Equal k-mer gluer faced a difficult situation in graph. Equal k-mers were ignored.");
				return;
			}
			// what is it?!
//			if(pos1 + pos2 >= graph_.length(edge1)) {
//				size_t tmp = pos1;
//				pos1 = graph_.length(edge1) - pos2;
//				pos2 = graph_.length(edge1) - tmp;
//			}
			if(pos1 > pos2) {
				size_t tmp = pos1;
				pos1 = pos2;
				pos2 = tmp;
			}
			TRACE("Edge1 " << graph_.int_id(edge1) << " will be splitted");
			pair<EdgeId, EdgeId> split_edges = graph_.SplitEdge(edge1, pos1 + 1);
			TRACE("Splitted pair was created");
			TRACE("New edge1: " << graph_.int_id(split_edges.first) << ", length: " << graph_.length(split_edges.first));
			TRACE("New edge2: " << graph_.int_id(split_edges.second) << ", length: " << graph_.length(split_edges.second));
			edge1 = split_edges.first;
			edge2 = graph_.conjugate(split_edges.second);
			pos2 -= pos2 - pos1 - 1;
			VERIFY(pos2 >= 0);
		}
		EdgeId se1 = ExtractShortEdge(edge1, pos1);
		EdgeId se2 = ExtractShortEdge(edge2, pos2);
		VERIFY(graph_.EdgeNucls(se1) == graph_.EdgeNucls(se2));
		GlueEqualEdges(se1, se2);
	}

	void SafelyGlueEdges(EdgeId e1, EdgeId e2){
        // e1 -> e2
		vector<EdgeId> forbidden_edges = {e1, e2};
        EdgeGluer(graph_).MoveEdgesFromVertexToVertex(graph_.EdgeStart(e1),
        		graph_.EdgeStart(e2), forbidden_edges);
        EdgeGluer(graph_).MoveEdgesFromVertexToVertex(graph_.EdgeEnd(e1),
        		graph_.EdgeEnd(e2), forbidden_edges);
        graph_.GlueEdges(e1, e2);
	}

	void GlueEqualEdges(EdgeId edge1, EdgeId edge2) {
		SafelyGlueEdges(edge1, edge2);
	}

public:
	EqualSequencesGluer(Graph &graph, conj_graph_pack::index_t &index): graph_(graph), index_(index) {
	}

	Sequence get(EdgeId e, size_t pos) const {
		return graph_.EdgeNucls(e).subseq(pos, pos + graph_.k() + 1);
	}

	void GlueEqualKmers() {
		size_t cnt = 0;
		for(auto it = graph_.SmartEdgeBegin(); !it.IsEnd(); ++it) {
			Sequence nucls = graph_.EdgeNucls(*it);
			runtime_k::RtSeq kmer = nucls.start<runtime_k::RtSeq>(graph_.k() + 1) >> 'A';
			for(size_t i = graph_.k(); i < graph_.length(*it); i++) {
				kmer = kmer << graph_.EdgeNucls(*it)[i];
				if(!index_.contains(kmer)) {
					continue;
				}
				pair<EdgeId, size_t> pos = index_.get(kmer);
				if(pos.first != *it || pos.second != i - graph_.k()) {
					GlueEqualEdgeParts(pos.first, pos.second, *it, i - graph_.k());
					cnt++;
					break;
				}
			}
		}
		INFO(cnt << " kmers glued");
	}

private:
	DECL_LOGGER("EqualSequencesGluer");
};

}

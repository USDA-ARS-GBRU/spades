#pragma once

#include "environment.hpp"
#include "compare_standard.hpp"
#include "coloring.hpp"

namespace online_visualization {

class CapEnvironmentManager;
/*
 * Cap Environment is designed to handle operations on fixed
 * set of genomes
 */
class CapEnvironment : public Environment {
  friend class CapEnvironmentManager;

 protected:
  typedef debruijn_graph::ConjugateDeBruijnGraph Graph;
  typedef Graph::VertexId VertexId;
  typedef Graph::EdgeId EdgeId;

  typedef debruijn_graph::graph_pack<Graph, runtime_k::RtSeq> RtSeqGraphPack;
  typedef debruijn_graph::graph_pack<Graph, cap::LSeq> LSeqGraphPack;

  typedef cap::ColorHandler<Graph> ColorHandler;

  std::string name_;
  std::string dir_;
  std::string description_;

  // Sequence of Ks which were used to refine current genome
  std::vector<unsigned> k_history_;
  // History of number of studied genomes (for case of adding genomes in the middle of pipeline)
  std::vector<unsigned> num_genomes_history_;

  // Paths on fs
  std::vector<std::string> init_genomes_paths_;
  // Genome sequences themselves. Yes, it may be lots of GBs.
  std::vector<std::shared_ptr<Sequence> > genomes_;
  std::vector<std::string> genomes_names_;

  std::shared_ptr<RtSeqGraphPack> gp_rtseq_;
  std::shared_ptr<LSeqGraphPack> gp_lseq_;

  std::shared_ptr<ColorHandler> coloring_;

  // Aliases to GraphPack parts:
  //
  Graph *graph_;
  EdgesPositionHandler<Graph> *edge_pos_;
	IdTrackHandler<Graph> *int_ids_;
  
  // Information concerning the default way to write out info about diversities
  std::string event_log_path_;
  // Either "w" or "a", and using the "a" mode during the environment load file
  // will be anyway recreated (and purged) though.
  std::string event_log_file_mode_;

  // Environment Manager for complex methods on this Environment
  std::shared_ptr<CapEnvironmentManager> manager_;
  
 public:
  static const size_t kNoGraphK = -1;
  const std::string kDefaultGPWorkdir;

  CapEnvironment(const std::string &name, /*const std::string base_path, */const std::string &description = "")
      : Environment(name, cap_cfg::get().cache_root + "/env_" + name),
        name_(name),
        dir_(cap_cfg::get().cache_root + "/env_" + name),
        //base_path_(base_path),
        description_(description),
        k_history_(),
        num_genomes_history_(),
        init_genomes_paths_(),
        gp_rtseq_(),
        gp_lseq_(),
        coloring_(),
        graph_(NULL),
        edge_pos_(NULL),
        int_ids_(NULL),
        event_log_path_(dir_ + "/" + cap_cfg::get().default_log_filename),
        event_log_file_mode_(cap_cfg::get().default_log_file_mode),
        manager_(std::make_shared<CapEnvironmentManager>(this)),
        kDefaultGPWorkdir("./tmp") {
    cap::utils::MakeDirPath(dir_);
  }

  void CheckConsistency() const {
    VERIFY(gp_rtseq_ == NULL || gp_lseq_ == NULL);
    bool have_any_gp = gp_rtseq_ != NULL || gp_lseq_ != NULL;
    if (have_any_gp) {
      VERIFY(graph_ != NULL);
      VERIFY(edge_pos_ != NULL);
      VERIFY(int_ids_ != NULL);
    }
  }

  size_t GetGraphK() const {
    if (gp_rtseq_ == NULL && gp_lseq_ == NULL) {
      return kNoGraphK;
    }

    return graph_->k();
  }
  bool LSeqIsUsed() const {
    return UseLSeqForThisK(GetGraphK());
  }

  // Method defining of we use RtSeq or LSeq for particular K
  bool UseLSeqForThisK(unsigned k) const {
    return k > 99;
  }

  const std::string &name() const {
    return name_;
  }
  const std::string &dir() const {
    return dir_;
  }
  const Graph &graph() const {
    return *graph_;
  }
  const EdgesPositionHandler<Graph> &edge_pos() const {
    return *edge_pos_;
  }
  const ColorHandler &coloring() const {
    return *coloring_;
  }
  const std::string &event_log_path() {
    return event_log_path_;
  }
  const std::string &event_log_file_mode() {
    return event_log_file_mode_;
  }
  CapEnvironmentManager &manager() const {
    return *manager_;
  }

};
}

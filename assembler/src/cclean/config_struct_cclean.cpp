//***************************************************************************
//* Copyright (c) 2011-2013 Saint-Petersburg Academic University
//* All Rights Reserved
//* See file LICENSE for details.
//****************************************************************************

#include "config_struct_cclean.hpp"
#include "openmp_wrapper.h"

void load(cclean_config& cfg, boost::property_tree::ptree const& pt) {
  using config_common::load;
  load(cfg.mismatch_threshold, pt, "mismatch_threshold");
  load(cfg.aligned_part_fraction, pt, "aligned_part_fraction");
  load(cfg.output_file, pt, "output_file");
  load(cfg.output_bed, pt, "output_bed");
  load(cfg.nthreads, pt, "nthreads");

  load(cfg.count_split_buffer, pt, "count_split_buffer");
  load(cfg.input_working_dir, pt, "input_working_dir");

  // Fix number of threads according to OMP capabilities.
  cfg.nthreads = std::min(cfg.nthreads, (unsigned)omp_get_max_threads());
  // Inform OpenMP runtime about this :)
  omp_set_num_threads(cfg.nthreads);
}

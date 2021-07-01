// Copyright (c) 2011-2019 Columbia University, System Level Design Group
// SPDX-License-Identifier: Apache-2.0

#ifndef __SYSTEM_HPP__
#define __SYSTEM_HPP__

#include "mindfuzz.hpp"
#include "mindfuzz_directives.hpp"

#include "esp_templates.hpp"

// determine mem size in bytes
#if (FX_WIDTH == 16)
// float still takes 4 bytes
const size_t MEM_SIZE = 1284864 / (DMA_WIDTH/8);
#else // 32
const size_t MEM_SIZE = 2569728 / (DMA_WIDTH/8);
#endif // 16 vs 32

// old 
//const size_t MEM_SIZE = 139198464 / (DMA_WIDTH/8);

#include "core/systems/esp_system.hpp"

#ifdef CADENCE
#include "mindfuzz_wrap.h"
#endif

class system_t : public esp_system<DMA_WIDTH, MEM_SIZE>
{
public:

    // ACC instance
#ifdef CADENCE
    mindfuzz_wrapper *acc;
#else
    mindfuzz *acc;
#endif

    // Constructor
    SC_HAS_PROCESS(system_t);
    system_t(sc_module_name name)
        : esp_system<DMA_WIDTH, MEM_SIZE>(name)
    {
        // ACC
#ifdef CADENCE
        acc = new mindfuzz_wrapper("mindfuzz_wrapper");
#else
        acc = new mindfuzz("mindfuzz_wrapper");
#endif
        // Binding ACC
        acc->clk(clk);
        acc->rst(acc_rst);
        acc->dma_read_ctrl(dma_read_ctrl);
        acc->dma_write_ctrl(dma_write_ctrl);
        acc->dma_read_chnl(dma_read_chnl);
        acc->dma_write_chnl(dma_write_chnl);
        acc->conf_info(conf_info);
        acc->conf_done(conf_done);
        acc->acc_done(acc_done);
        acc->debug(debug);

        /* <<--params-default-->> */
        num_loads = 223;
        batches_perload = 1;
        tsamps_perbatch = 16;
        num_windows = 1;
        elecs_perwin = 32;
        hiddens_perwin = 6;
        do_init = true;
        thresh_batches = 1875;
        backprop_batches = 1875;
        shift_thresh = 12;
        shift_tsamps = 4;
        shift_elecs = 5;
        shift_gamma = 6;
        shift_alpha = 12;
        thresh_mult = (TYPE)6.0;

// might be useful if we need to do float conversions
/*
#ifdef split_LR
        // version with split learning rate.
        // used for fixed point
        // learning_rate * shift_A * shift_down_C = non-split learning rate
#if (FX_WIDTH == 16)
        learning_rate = TYPE(((float)numerator_B) / ((float)225));
#else // 32
        learning_rate = TYPE(((float)numerator_B) / ((float)703125));
#endif // 16 vs 32

#elif (USE_FX == 0)
        // using cynw_cm_float - need to use the correct conversion function

        // learning rate math is the same as single learning rate below
        float overall_learning_rate = ((float)0.000002) / ((float)tsamps_perbatch) / ((float)elecs_perwin);

        // first convert to a 32b cynw_cm_float
        cynw_cm_float<8, 32, CYNW_BEST_ACCURACY, CYNW_NEAREST, 1> over_lr_32 = cynw_cm_float<8, 32, CYNW_BEST_ACCURACY, CYNW_NEAREST, 1>(overall_learning_rate);

        // then construct the object of TYPE (the actual cynw_cm_float type we're using)
        // with the 32b cynw_cm_float as the input
        learning_rate = TYPE(over_lr_32);

        //learning_rate = FPDATA::float_to_cynw_cm_float(overall_learning_rate, sc_uint<5> exc);
        //learning_rate = FPDATA.float_to_cynw_cm_float(overall_learning_rate, sc_uint<5> exc);
#else
        // version with single learning rate.
        // note that calculus factor of 2 has been manually applied here
        learning_rate = TYPE(((float)0.000002) / ((float)tsamps_perbatch) / ((float)elecs_perwin));
#endif
*/
   }

    // Processes

    // Configure accelerator
    void config_proc();

    // Load internal memory
    void load_memory();

    // Dump internal memory
    void dump_memory();

    // Validate accelerator results
    int validate();

    // Accelerator-specific data
    /* <<--params-->> */
    int32_t num_loads;
    int32_t batches_perload;
    int32_t tsamps_perbatch;
    int32_t num_windows;
    int32_t elecs_perwin;
    int32_t hiddens_perwin;
    bool do_init;
    int32_t thresh_batches;
    int32_t backprop_batches;
    int32_t shift_thresh;
    int32_t shift_tsamps;
    int32_t shift_elecs;
    int32_t shift_gamma;
    int32_t shift_alpha;
    TYPE thresh_mult;

    uint32_t in_words_adj;
    uint32_t out_words_adj;
    uint32_t in_size;
    uint32_t out_size;
    float *in;
    float *out;
    float *gold;

    // Other Functions
};

#endif // __SYSTEM_HPP__

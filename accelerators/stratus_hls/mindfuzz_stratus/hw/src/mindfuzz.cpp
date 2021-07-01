// Copyright (c) 2011-2019 Columbia University, System Level Design Group
// SPDX-License-Identifier: Apache-2.0
//
// modified by CRT, Columbia University, Bioelectronic Systems Lab

#include "mindfuzz.hpp"
#include "mindfuzz_directives.hpp"

// Functions

#include "mindfuzz_functions.hpp"

// Processes

void mindfuzz::load_input()
{

    // Reset
    {
        HLS_PROTO("load-reset");

        this->reset_load_input();

        // explicit PLM ports reset if any
        // this would be necessary if we accessed PLMs explicitely
        // by default, they are mapped to arrays so we read and writ
        // with indices just like arrays

        // User-defined reset code

        wait();
    }

    // Config
    uint16_t num_loads;
    uint8_t batches_perload;
    uint8_t tsamps_perbatch;
    uint16_t num_windows;
    uint8_t elecs_perwin;
    uint8_t hiddens_perwin;
    bool do_init;
    uint16_t thresh_batches;
    uint16_t backprop_batches;
    uint8_t shift_thresh;
    uint8_t shift_tsamps;
    uint8_t shift_elecs;
    uint8_t shift_gamma;
    uint8_t shift_alpha;
    TYPE thresh_mult;

    // declare some necessary variables
    // int32_t load_batches;
    {
        HLS_PROTO("load-config");

        cfg.wait_for_config(); // config process
        conf_info_t config = this->conf_info.read();

        // User-defined config code
        num_loads = config.num_loads;
        batches_perload = config.batches_perload;
        tsamps_perbatch = config.tsamps_perbatch;
        num_windows = config.num_windows;
        elecs_perwin = config.elecs_perwin;
        hiddens_perwin = config.hiddens_perwin;
        do_init = config.do_init;
        thresh_batches = config.thresh_batches;
        backprop_batches = config.backprop_batches;
        shift_thresh = config.shift_thresh;
        shift_tsamps = config.shift_tsamps;
        shift_elecs = config.shift_elecs;
        shift_gamma = config.shift_gamma;
        shift_alpha = config.shift_alpha;
        thresh_mult = a_read(config.thresh_mult);
    }

    // Load
    {
        HLS_PROTO("load-dma");
        wait();

        bool ping = true;
        uint32_t offset = 0;

        // Batching
        for (uint16_t b = 0; b < num_loads; b++)
        {
            wait();

// for 16b data, DMA_WORD_PER_BEAT = 2
// DMA_WORD_PER_BEAT would equal 0 if WORD size > BEAT size
#if (DMA_WORD_PER_BEAT == 0)
            uint32_t length = num_windows*elecs_perwin*tsamps_perbatch*batches_perload;
#else
            // broke up this computation and added some waits in order to improve schedule
            uint32_t length_dum = num_windows*elecs_perwin*tsamps_perbatch*batches_perload;
            wait();
            uint32_t length = round_up(length_dum, DMA_WORD_PER_BEAT);
#endif
            wait();
            // Chunking - in this case, no chunk, so one iteration per batch
            for (int rem = length; rem > 0; rem -= PLM_IN_WORD)
            {
                wait();
                // Configure DMA transaction
                uint32_t len = rem > PLM_IN_WORD ? PLM_IN_WORD : rem;
#if (DMA_WORD_PER_BEAT == 0)
                // data word is wider than NoC links
                dma_info_t dma_info(offset * DMA_BEAT_PER_WORD, len * DMA_BEAT_PER_WORD, DMA_SIZE);
#else
                dma_info_t dma_info(offset / DMA_WORD_PER_BEAT, len / DMA_WORD_PER_BEAT, DMA_SIZE);
#endif
                offset += len;

                //{
                //HLS_PROTO("load-dma");

                this->dma_read_ctrl.put(dma_info);

#if (DMA_WORD_PER_BEAT == 0)
                // data word is wider than NoC links
                for (uint16_t i = 0; i < len; i++)
                {
                    sc_dt::sc_bv<DATA_WIDTH> dataBv;

                    for (uint16_t k = 0; k < DMA_BEAT_PER_WORD; k++)
                    {
                        dataBv.range((k+1) * DMA_WIDTH - 1, k * DMA_WIDTH) = this->dma_read_chnl.get();
                        wait();
                    }

                    // Write to PLM
                    if (ping)
                        plm_in_ping[i] = dataBv.to_int64();
                    else
                        plm_in_pong[i] = dataBv.to_int64();
                }
#else
                for (uint16_t i = 0; i < len; i += DMA_WORD_PER_BEAT)
                {
                    // TODO what is this?
                    HLS_BREAK_DEP(plm_in_ping);
                    HLS_BREAK_DEP(plm_in_pong);

                    sc_dt::sc_bv<DMA_WIDTH> dataBv;

                    dataBv = this->dma_read_chnl.get();
                    wait();

                    // Write to PLM (all DMA_WORD_PER_BEAT words in one cycle)
                    for (uint16_t k = 0; k < DMA_WORD_PER_BEAT; k++)
                    {
                        HLS_UNROLL_SIMPLE;
                        if (ping)
                            plm_in_ping[i + k] = dataBv.range((k+1) * DATA_WIDTH - 1, k * DATA_WIDTH).to_int64();
                        else
                            plm_in_pong[i + k] = dataBv.range((k+1) * DATA_WIDTH - 1, k * DATA_WIDTH).to_int64();
                    }
                }
#endif

                //} // end of protocol region

                this->load_compute_handshake();
                ping = !ping;
                // chunk complete
            }
            // batch complete
        }
    }

    // Conclude
    {
        this->process_done();
    }
}

void mindfuzz::compute_kernel()
{
    // Reset
    {
        HLS_PROTO("compute-reset");

        this->reset_compute_kernel();

        // explicit PLM ports reset if any

        // User-defined reset code

        wait();
    }

    // Config
    uint16_t num_loads;
    uint8_t batches_perload;
    uint8_t tsamps_perbatch;
    uint16_t num_windows;
    uint8_t elecs_perwin;
    uint8_t hiddens_perwin;
    bool do_init;
    uint16_t thresh_batches;
    uint16_t backprop_batches;
    uint8_t shift_thresh;
    uint8_t shift_tsamps;
    uint8_t shift_elecs;
    uint8_t shift_gamma;
    uint8_t shift_alpha;
    TYPE thresh_mult;
   
    {
        HLS_PROTO("compute-config");

        cfg.wait_for_config(); // config process
        conf_info_t config = this->conf_info.read();

        // User-defined config code
        num_loads = config.num_loads;
        batches_perload = config.batches_perload;
        tsamps_perbatch = config.tsamps_perbatch;
        num_windows = config.num_windows;
        elecs_perwin = config.elecs_perwin;
        hiddens_perwin = config.hiddens_perwin;
        do_init = config.do_init;
        thresh_batches = config.thresh_batches;
        backprop_batches = config.backprop_batches;
        shift_thresh = config.shift_thresh;
        shift_tsamps = config.shift_tsamps;
        shift_elecs = config.shift_elecs;
        shift_gamma = config.shift_gamma;
        shift_alpha = config.shift_alpha;
        thresh_mult = a_read(config.thresh_mult);
    }

    // some info that the functions will  need
    uint8_t total_tsamps = batches_perload*tsamps_perbatch;
    uint16_t num_electrodes = num_windows*window_size;


    // Compute

    // initialize - note that this will happen at beginning of accel invocation,
    // because the do_init config param must be configured.
    if (do_init) {

        // initialize sigma estimates
    
        // initial value for each
        TYPE initial_sigma = (TYPE)0.0;

        for (uint16_t window = 0; window < num_windows; window++) {
            uint16_t window_offset = window * elecs_perwin;
            for (uint8_t elec = 0; elec < elecs_perwin; elec++) {
                plm_sigma[window_offset + elec] = a_write(initial_sigma);
            }
        }

        // reset the counters to keep track of the number of batches done for thresholding and backprop
        thresh_complete = 0;
        backprop_complete = 0;
    }

    bool ping = true;

    {
        for (uint16_t b = 0; b < num_loads; b++)
        {

            // no-chunk code
            // see below for old code with chunking
            this->compute_load_handshake();

	    // train threshold
	    if (thresh_complete < thresh_batches) {

		thresh_train(num_windows,
                             elecs_perwin,
                             shift_thresh,
                             ping);

                thresh_complete++;

	    }

            // on the first load after completing thresh training, do some additional post-proc
            if (thresh_complete == thresh_batches {
 
                thresh_calculate(num_windows,
                                 elecs_perwin,
                                 shift_elecs,
                                 thresh_mult);

                // initialize weights and mu

	        // initial value for each weight
		// start with 1 and apply a shift based on electrodes per window
		TYPE initial_weight = (TYPE)1.0;
                initial_weight = bitshift(initial_weight, shift_elecs, false);

                // TODO declare and calculate W1 size above if it's needed for autoenc
		// initialize W1
		for (uint32_t weight = 0; weight < W1_size; weight++) {
		    plm_out[weight] =
			a_write(initial_weight);
		}

                // initialize mu

                TYPE winsig;
                uint16_t window_offset;

                for (uint16_t window = 0; window < num_windows; window++) {

                    window_offset = window*hiddens_perwin;

                    // get the average simga for this window
                    winsig = a_read(plm_winsigma[window]);

                    // bit shift by number of electrodes
                    winsig = bitshift(winsig, shift_elecs, false);

                    for (uint8_t hidden = 0; hidden < hiddens_perwin; hidden++) {

                        plm_mu[window_offset + hidden] = a_write(winsig);
                    }
                }

                thresh_complete++;
            }

            // proceed
            if (thresh_complete > thresh_batches) {

                relevant(num_windows,
                         elecs_perwin,
                         num_electrodes,
                         total_tsamps,
                         ping);

                // run backprop for each compute batch in this load batch
                for (uint16_t batch = 0; batch < batches_perload; batch++) {

                    // pass relevant parameters like flag, pingpong, and how many training batches have run
                    // within autoenc, we will check whether to run backprop or just forward.
                    // backprop will access weights, training data, directly (they are in PLMs)
                    autoenc(
                            // TODO
                           );

                    // this autoenc batch done
                }
            }

            ping = !ping;
            // this data load done

/*
            uint32_t in_length = num_windows*elecs_perwin*tsamps_perbatch*batches_perload;

            // since we have a chunking factor of 1, this only does one iteration
            // TODO add parameter to pass to backprop() so that we can
            // process a chunk by sections of the electrode array
            for (int in_rem = in_length; in_rem > 0; in_rem -= PLM_IN_WORD)
            {

                uint32_t in_len  = in_rem  > PLM_IN_WORD  ? PLM_IN_WORD  : in_rem;

                
            }
*/

        }
        // all loads done 
        // TODO update for new output schedule
        this->compute_store_handshake();
    }
    // Conclude
    {
        this->process_done();
    }
}


void mindfuzz::store_output()
{
    // Reset
    {
        HLS_PROTO("store-reset");

        this->reset_store_output();

        // explicit PLM ports reset if any

        // User-defined reset code

        wait();
    }

    // Config
    uint16_t num_loads;
    uint8_t batches_perload;
    uint8_t tsamps_perbatch;
    uint16_t num_windows;
    uint8_t elecs_perwin;
    uint8_t hiddens_perwin;
    bool do_init;
    uint16_t thresh_batches;
    uint16_t backprop_batches;
    uint8_t shift_thresh;
    uint8_t shift_tsamps;
    uint8_t shift_elecs;
    uint8_t shift_gamma;
    uint8_t shift_alpha;
    TYPE thresh_mult;

    // declare some necessary variables
    // int32_t load_batches;
    {
        HLS_PROTO("store-config");

        cfg.wait_for_config(); // config process
        conf_info_t config = this->conf_info.read();
        
        // User-defined config code
        num_loads = config.num_loads;
        batches_perload = config.batches_perload;
        tsamps_perbatch = config.tsamps_perbatch;
        num_windows = config.num_windows;
        elecs_perwin = config.elecs_perwin;
        hiddens_perwin = config.hiddens_perwin;
        do_init = config.do_init;
        thresh_batches = config.thresh_batches;
        backprop_batches = config.backprop_batches;
        shift_thresh = config.shift_thresh;
        shift_tsamps = config.shift_tsamps;
        shift_elecs = config.shift_elecs;
        shift_gamma = config.shift_gamma;
        shift_alpha = config.shift_alpha;
        thresh_mult = a_read(config.thresh_mult);
    }

    // Store
    {
        HLS_PROTO("store-dma");
        
        // deleted pingpong

// compute the DMA offset due to input data
#if (DMA_WORD_PER_BEAT == 0)
        uint32_t store_offset = (num_windows*elecs_perwin*tsamps_perbatch*batches_perload) * num_loads;
#else
        uint32_t store_offset = round_up(num_windows*elecs_perwin*tsamps_perbatch*batches_perload, DMA_WORD_PER_BEAT) * num_loads;
#endif
        uint32_t offset = store_offset;

        wait();
// length of data to be stored
#if (DMA_WORD_PER_BEAT == 0)
        uint32_t length = num_windows*hiddens_perwin*elecs_perwin;
#else
        // broke up this computation and added some waits in order to improve schedule
        uint32_t length_dum = num_windows*hiddens_perwin*elecs_perwin;
        wait();
        uint32_t length = round_up(length_dum, DMA_WORD_PER_BEAT);
#endif
        wait();

// deleted batching since we only store at the end

        // this will only happen at the end
        this->store_compute_handshake();

        // deleted chunking - see here that the variable len from chunking is simply length
        uint32_t len = length;

// stuff that used to be in chunking loop
#if (DMA_WORD_PER_BEAT == 0)
        // data word is wider than NoC links
        dma_info_t dma_info(offset * DMA_BEAT_PER_WORD, len * DMA_BEAT_PER_WORD, DMA_SIZE);
#else
        dma_info_t dma_info(offset / DMA_WORD_PER_BEAT, len / DMA_WORD_PER_BEAT, DMA_SIZE);
#endif
        offset += len;

        this->dma_write_ctrl.put(dma_info);

#if (DMA_WORD_PER_BEAT == 0)
        // data word is wider than NoC links
        for (uint16_t i = 0; i < len; i++)
        {
            // Read from PLM
            sc_dt::sc_int<DATA_WIDTH> data;
            wait();

            // deleted pingpong
            data = plm_out[i];

            sc_dt::sc_bv<DATA_WIDTH> dataBv(data);

            uint16_t k = 0;
            for (k = 0; k < DMA_BEAT_PER_WORD - 1; k++)
            {
                this->dma_write_chnl.put(dataBv.range((k+1) * DMA_WIDTH - 1, k * DMA_WIDTH));
                wait();
            }
            // Last beat on the bus does not require wait(), which is
            // placed before accessing the PLM
            this->dma_write_chnl.put(dataBv.range((k+1) * DMA_WIDTH - 1, k * DMA_WIDTH));
        }
#else
        for (uint16_t i = 0; i < len; i += DMA_WORD_PER_BEAT)
        {
            sc_dt::sc_bv<DMA_WIDTH> dataBv;

            // Read from PLM
            wait();
            for (uint16_t k = 0; k < DMA_WORD_PER_BEAT; k++)
            {
                HLS_UNROLL_SIMPLE;

                // deleted pingpong
                dataBv.range((k+1) * DATA_WIDTH - 1, k * DATA_WIDTH) = plm_out[i + k];
            }

            this->dma_write_chnl.put(dataBv);
        }
#endif

// commented out chunking since we will store all outputs at once
/*
        // Chunking - we have a chunking factor of one so this only runs once
        for (int rem = length; rem > 0; rem -= PLM_OUT_WORD)
        {

            // Configure DMA transaction
            uint32_t len = rem > PLM_OUT_WORD ? PLM_OUT_WORD : rem;

            // this is where the removed stuff went
            // end of chunk
        }
*/

    }

    // Conclude
    {
        this->accelerator_done();
        this->process_done();
    }
}

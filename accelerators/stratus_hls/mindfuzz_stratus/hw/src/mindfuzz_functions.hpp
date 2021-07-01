// Copyright (c) 2011-2019 Columbia University, System Level Design Group
// SPDX-License-Identifier: Apache-2.0

// Optional application-specific helper functions

// Caleb Rees Tulloss
// Bioelectronic Systems Lab
// helper functions for autoencoder weight updates, threshold updates, detection

// computes the next-largest power of 2 for a fixed- or floating-point number
uint8_t mindfuzz::Po2(TYPE data) {

    uint8_t shift = 0;
    TYPE power = 1.0;

    while (power < data) {
        shift++;
        power *= 2;
    }

    return shift;

}

// performs bit shift on fixed- or floating-point data
TYPE mindfuzz::bitshift(TYPE data, uint8_t shift, bool left) {

#if (USE_FX == 1) // fixed point

    // cast to integer
    FPDATA_WORD data_int = fp2int<TYPE, WORD_SIZE>(data);

    // shifting left
    if (left) {
        data_int = data_int << shift;
    }
    
    // shifting right
    else {
        data_int = data_int >> shift;
    }

    // cast back to fixed point
    data = int2fp<TYPE, WORD_SIZE>(data);

#else // floating point

    // shifting left
    if (left) {
        for (uint8_t s = 0; s < shift; s++) {
            data *= 2;
        }
    }

    // shifting right
    else {
        for (uint8_t s = 0; s < shift; s++) {
            data *= 0.5;
        }
    }

#endif // fixed vs floating

    return data;

}


// thresh_train: incrementally updates an estimate of the mean absolute deviation
// for each electrode, which will later be used for thresholding
void mindfuzz::thresh_train(uint16_t num_windows,
                            uint8_t elecs_perwin,
                            uint8_t shift_thresh,
                            ping) {

    uint16_t total_offset;
    uint16_t index;

    TYPE data;
    TYPE sigma;

    for (uint16_t window = 0; window < num_windows; window++) {
        
        total_offset = window * elecs_perwin;

        for (uint8_t elec = 0; elec < elecs_perwin; elec++) {

            // determine electrode index
            index = total_offset + elec;

            // acquire sample for this electrode
            if (ping) {
                data = a_read(plm_in_ping[index]);
            }
            else {
                data = a_read(plm_in_pong[index]);
            }

            // take absolute value
            if (data < 0.0) {
                data = data * -1.0;
            }

            // acquire current estimate of sigma for this electrode
            sigma = a_read(plm_sigma[index]);

            // now compute s = (1-shift)*s + shift*x,
            // where x is the new sample

            // start with subtracting s
            sigma = sigma - bitshift(sigma, shift_thresh, false);

            // scale the new data point
            data = bitshift(data, shift_thresh, false);

            // add new data point to the estimate
            sigma += data;

            // write the result
            plm_sigma[index] = a_write(sigma);

        }
    }
}


// thresh_calculate: post-processes the sigma estimate for each electrode,
// by calculating mean sigma for each window, and scaling each elec's
// sigma by 6 to be used for thresholding
void mindfuzz::thresh_calculate(uint16_t num_windows,
                                uint8_t elecs_perwin,
                                uint8_t shift_elecs,
                                TYPE thresh_mult) {

    uint16_t total_offset;
    uint16_t index;

    TYPE sigma;
    TYPE sum_sigma;

    for (uint16_t window = 0; window < num_windows; window++) {
        
        total_offset = window * elecs_perwin;

        // reset sum of sigma for this window
        sum_sigma = 0.0;

        for (uint8_t elec = 0; elec < elecs_perwin; elec++) {

            // determine electrode index
            index = total_offset + elec;

            // acquire sigma for this electrode
            sigma = a_read(plm_sigma[index]);

            // add it to the running sum
            sum_sigma += sigma;

            // scale it up to be used for thresholding
            // thresholding will be done on max-min for each elec
            // so use 6*sigma (3*sigma on each side)
            sigma *= thresh_mult;

            // write the threshold
            plm_sigma[index] = a_write(sigma);

        }

        // calculate mean sigma
        sum_sigma = bitshift(sum_sigma, shift_elecs, false);

        // write it
        plm_winsigma[window] = a_write(sum_sigma);

    }

}


// relevant: flags data as either spike or noise
void mindfuzz::relevant(uint16_t num_windows,
                        uint8_t elecs_perwin,
                        uint16_t num_electrodes,
                        uint8_t total_tsamps,
                        bool ping) {


    // TODO come back to these declarations - can I define the max / min as plms?

    TYPE data;
    TYPE maxmin;
    TYPE thresh;


    uint32_t samp_offset;
    uint16_t window_offset;
    uint32_t total_offset;

    // first, process all time samples to find max and min for each elec
    for (uint8_t samp = 0; samp < total_tsamps; samp++) {

        samp_offset = samp * num_electrodes;
        
        for (uint16_t window = 0; window < num_windows; window++) {

            window_offset = window * window_size;
            total_offset = samp_offset + window_offset;

            for (uint8_t elec = 0; elec < window_size; elec++) {

                // on first sample for each elec, reset the max and min
                if (samp == 0) {
                    plm_max[window_offset + elec] = a_write(0.0);
                    plm_min[window_offset + elec] = a_write(0.0);
                }

                if (ping) {
                    data = a_read(plm_in_ping[total_offset + elec]);
                }
                else {
                    data = a_read(plm_in_pong[total_offset + elec]);
                }

                // compare data for this sample with existing max and min
                if (data > a_read(max[window_offset + elec])) {
                    plm_max[window_offset + elec] = a_write(data);
                }
                else if (data < a_read(min[window_offset + elec])) {
                    plm_min[window_offset + elec] = a_write(data);
                }

                // done with this sample for this electrode
            }
            // done with this sample for all electrodes in this window
        }
        // done with this sample for all electrodes in all windows
    }

    // now post-process each window to check max and min
    for (uint16_t window = 0; window < num_windows; window++) {

        window_offset = window * window_size;

        // reset flag
        plm_flag[window] = false;

        // check max and min for each elec
        for (uint8_t elec = 0; elec < window_size; elec++) {
          
            // calculate max-min
            maxmin = a_read(plm_max[window_offset + elec]) - a_read(plm_min[window_offset + elec]);

            // acquire thresh for this electrode
            thresh = a_read(plm_sigma[window_offset + elec]);

            if (maxmin > thresh) {
                // flag the window
                flag[window] = true;
            }
        }
        // done with this window
    }

    // done
}

// backprop function with multiple hidden components in series
void mindfuzz::backprop(uint8_t shift_batch,
                        uint8_t shift_winsize,
                        uint8_t shift_gamma,
                        uint8_t shift_alpha,
                        uint8_t tsamps_perbatch,
                        uint8_t input_dimension,
                        uint8_t hiddens_perwin,
                        uint16_t num_windows,
                        uint32_t W_size,
                        uint8_t batch,
                        bool flag[],
                        bool ping) {

    // single-tsamp data for all electrodes - size e.g. 4 * 32 = 256
    uint16_t num_electrodes = num_windows*input_dimension;
    
    // TODO FLATTEN THIS?
    // if so, need to add a_read to where input is read
    FPDATA_WORD elecdata[PLM_ELEC_WORD];

    // some offsets useful for indexing
    uint32_t samp_offset;
    uint32_t window_offset_weights1;
    uint16_t window_offset_layer1;
    uint16_t window_offset_input;

    // PLM access offset for input data
    // for sw-only version, added offset to take into account load batch
    uint32_t batch_offset = num_electrodes * tsamps_perbatch * batch;

    // useful for arithmetic
    uint16_t W1_singlewindow = hiddens_perwin*input_dimension;

    // iter accumulation variables for batched backprop
    // TODO rewrite to not use arbitrarily sized arrasy
    const uint32_t const_W1_size = CONST_NUM_WINDOWS * CONST_WINDOW_SIZE * CONST_HIDDENS_PERWIN;
    const uint16_t const_diff_size = CONST_NUM_WINDOWS * CONST_WINDOW_SIZE;
    const uint16_t const_act1_size = CONST_NUM_WINDOWS * CONST_HIDDENS_PERWIN;
    
    FPDATA_WORD dW1[const_W1_size];
    FPDATA_WORD dmu[const_act1_size];

    // temporary variables to store some results
    // forward pass: activation of layer 1 and difference between out and in
    // TODO rewrite to not use arbitrarily sized arrays
    FPDATA_WORD act1[const_act1_size];
    FPDATA_WORD out[const_diff_size];
    FPDATA_WORD diff[const_diff_size];
    
    // reset weight delta accumulation variables
    for (uint32_t i = 0; i < W_size; i++) {
        dW1[i] = a_write(0.0);
    }

    for (uint8_t samp = 0; samp < tsamps_perbatch; samp++) {

        // offset to access input data for this time samp
        samp_offset = batch_offset + samp*num_electrodes;

        // access input data for all windows from PLM
        // only place we need to worry about pingpong
        if (ping) {
            for (uint16_t elec = 0; elec < num_electrodes; elec++) {
                // this is a PLM access - can only UNROLL if has multiple ports
                elecdata[elec] = plm_in_ping[samp_offset + elec];
            }
        }
        else {
            for (uint16_t elec = 0; elec < num_electrodes; elec++) {
                // this is a PLM access - can only UNROLL if has multiple ports
                elecdata[elec] = plm_in_pong[samp_offset + elec];
            }
        }

        for (uint16_t window = 0; window < num_windows; window++) {
            // TODO UNROLL? - if so, need to fix the offsets

            // do backprop only on noise data
            if (!flag[window]) {

                // compute some offsets for loop indexing
                window_offset_weights1 = window*W1_singlewindow;
                window_offset_layer1 = window*hiddens_perwin;
                window_offset_input = window*input_dimension;
                

                // forward pass

                // dummy variable for input data to compare against
                TYPE temp_input;
                // dummy variables for layer 1 activation increment
                TYPE temp_act1;
                TYPE temp_incr;
                // dummy variable for output activation
                TYPE temp_out;
                // dummy variable for weight delta
                TYPE temp_dW1;

                // processing for each "hidden" is done in series
                for (uint8_t hidden = 0; hidden < hiddens_perwin; hidden++) {

                    // compute layer1 activation for this window
                    // reset activation accumulation variable for this sample
                    temp_act1 = (TYPE)0.0;

                    // mac
                    for (uint8_t in = 0; in < input_dimension; in++) {

                        // determine appropriate input for this hidden
                        if (hidden == 0) {
                            // hidden 0 uses electrode data
                            temp_input = a_read(elecdata[window_offset_input + in]);
                        }
                        else {
                            // subsequent hiddens use error from previous hidden
                            temp_input = a_read(diff[window_offset_input + in]);
                        }

                        // compute (FP) increment
                        temp_incr = a_read(plm_out[window_offset_weights1 +
                                hidden*input_dimension + in]) *
                            temp_input;
                        // add to accum variable
                        temp_act1 = temp_act1 + temp_incr;
                    }
                 
                    // update act1
                    act1[window_offset_layer1 + hidden] = a_write(temp_act1);

                    // compute outputs and differences (out - in)
                    // note no mac here bc the outputs are computed in series
                    // so the computation for each output is a scalar mult
                    for (uint8_t out = 0; out < input_dimension; out++) {
                    
                        // determine appropriate ground truth for this hidden
                        if (hidden == 0) {
                            // hidden 0 uses electrode data
                            temp_input = a_read(elecdata[window_offset_input + out]);
                        }
                        else {
                            // subsequent hiddens use error from previous hidden
                            temp_input = a_read(diff[window_offset_input + out]);
                        }
                   
                        // compute output
                        temp_out = 
                            a_read(plm_out[window_offset_weights1 + hidden*input_dimension + out]) *
                            a_read(act1[window_offset_layer1 + hidden]);
/*
                        if (samp == 0 && out == 0) {
                            ESP_REPORT_INFO("sample %d, hidden %d, electrode %d input is %0.16f", samp, hidden, out, float(temp_input));
                            ESP_REPORT_INFO("sample %d, hidden %d, electrode %d ouput is %0.16f", samp, hidden, out, float(temp_out));
                        }
*/

                        // subtract the ground truth difference
                        diff[window_offset_input + out] = a_write(temp_out - temp_input);
/*
                        if (samp == 0 && out == 0) {
                            ESP_REPORT_INFO("sample %d, hidden %d, electrode %d diff  is %0.16f", samp, hidden, out, float(a_read(diff[window_offset_input + out])));
                            ESP_REPORT_INFO("sample %d, hidden %d, electrode %d dW1   is %0.16f", samp, hidden, out, float(a_read(dW1[window_offset_weights1 + hidden*input_dimension + out])));
                        }
*/

                        // begin backprop: accumulate weight delta
                        
                        // acquire existing dW1
                        temp_dW1 = a_read(
                            dW1[window_offset_weights1 + hidden*input_dimension + out]);

                        // compute increment
                        // LEARNING RATE LOCATION A
#ifdef split_LR
/*
                        // bit shift the dW1 for this sample - mult version
                        temp_incr = a_read(diff[window_offset_input + out]) *
                            TYPE(shift_A * a_read(act1[window_offset_layer1 + hidden]));
*/
                        // bit shift the dW1 for this sample - bit shift version
                        temp_incr = (a_read(diff[window_offset_input + out]) >> bs_A) *
                            a_read(act1[window_offset_layer1 + hidden]);
#else
                        // regular version
                        // learning rate will be applied later during weight update
                        temp_incr = a_read(diff[window_offset_input + out]) *
                            a_read(act1[window_offset_layer1 + hidden]);
#endif

                        // update dW1
                        dW1[window_offset_weights1 + hidden*input_dimension + out] = 
                            a_write(temp_incr + temp_dW1);
/*
                        if (samp == 0 && out == 0) {
                            ESP_REPORT_INFO("sample %d, hidden %d, electrode %d incr  is %0.16f", samp, hidden, out, float(temp_incr));
                            ESP_REPORT_INFO("sample %d, hidden %d, electrode %d dW1_n is %0.16f", samp, hidden, out, float(a_read(dW1[window_offset_weights1 + hidden*input_dimension + out])));
                        }
*/

                    }
                }
            }
            // end of this window
        }
        // this sample is complete for this iter
    }
    // batch forward passes complete
    // all samples have now been processed,
    // and we are ready to perform a weight update for this iter
    TYPE temp_plmval;
    TYPE temp_incr;
    for (uint16_t window = 0; window < num_windows; window++) {
        // TODO UNROLL?

        // update weights only for noise data
        if (!flag[window]) {

            // compute some offsets for loop indexing
            window_offset_weights1 = window*W1_singlewindow;
            window_offset_layer1 = window*hiddens_perwin;
            window_offset_input = window*input_dimension;

            for (uint8_t hidden = 0; hidden < hiddens_perwin; hidden++) {
                
                // update W1
                for (uint8_t in = 0; in < input_dimension; in++) {

                    // acquire existing plmval
                    temp_plmval = a_read(
                        plm_out[window_offset_weights1 + hidden*input_dimension + in]);

                    // compute (FP) increment
                    // LEARNING RATE LOCATION B
                    // this one is the same whether we use split LR or not
                    // but value of learning_rate will be different
                    temp_incr = a_read(dW1[window_offset_weights1
                            + hidden*input_dimension + in]) * (learning_rate);
/*
                    if (in == 0) {
                        ESP_REPORT_INFO("hidden %d, input %d        weight is %0.16f", hidden, in, float(temp_plmval));
                        ESP_REPORT_INFO("hidden %d, input %d        deltaW is %0.16f", hidden, in, float(a_read(dW1[window_offset_weights1 + hidden*input_dimension + in])));
                        ESP_REPORT_INFO("hidden %d, input %d        dW*l_r is %0.16f", hidden, in, float(temp_incr));
                    }
*/
#ifdef split_LR
                    // LEARNING RATE LOCATION C
                    // scale up the current weight - mult version
                    //temp_plmval = temp_plmval * shift_up_C;
                    // scale up the current weight - bit shift version
                    temp_plmval = temp_plmval << bs_C;
/*
                    if (in == 0) {
                        ESP_REPORT_INFO("hidden %d, input %d scaled weight is %0.16f", hidden, in, float(temp_plmval));
                    }
*/
                    // do the increment
                    temp_plmval = temp_plmval - temp_incr;
/*
                    if (in == 0) {
                        ESP_REPORT_INFO("hidden %d, input %d incred weight is %0.16f", hidden, in, float(temp_plmval));
                    }
*/
                    // scale back down - mult version
                    //temp_plmval = temp_plmval * shift_down_C;
                    // scale back down - bit shift version
                    temp_plmval = temp_plmval >> bs_C;
/*
                    if (in == 0) {
                        ESP_REPORT_INFO("hidden %d, input %d new    weight is %0.16f", hidden, in, float(temp_plmval));
                    }
*/
#else
                    // calculate new weight
                    temp_plmval = temp_plmval - temp_incr;
/*
                    if (in == 0) {
                        ESP_REPORT_INFO("hidden %d, input %d new    weight is %0.16f", hidden, in, float(temp_plmval));
                    }
*/
#endif
                    // update plmval
                    plm_out[window_offset_weights1 + hidden*input_dimension + in] = 
                        a_write(temp_plmval);

                }
            }
        }
        // this window is now complete
    }
    // all windows complete
}

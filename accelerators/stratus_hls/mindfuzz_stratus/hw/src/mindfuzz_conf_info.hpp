// Copyright (c) 2011-2019 Columbia University, System Level Design Group
// SPDX-License-Identifier: Apache-2.0

#ifndef __MINDFUZZ_CONF_INFO_HPP__
#define __MINDFUZZ_CONF_INFO_HPP__

#include <systemc.h>

//
// Configuration parameters for the accelerator.
//
class conf_info_t
{
public:

    //
    // constructors
    //
    conf_info_t()
    {
        /* <<--ctor-->> */
        this->num_loads = 128;
        this->batches_perload = 1;
        this->tsamps_perbatch = 16;
        this->num_windows = 8;
        this->elecs_perwin = 32;
        this->hiddens_perwin = 6;
        this->do_init = true;
        this->thresh_batches = 1875;
        this->backprop_batches = 1875;
        this->shift_thresh = 12;
        this->shift_tsamps = 4;
        this->shift_elecs = 5;
        this->shift_gamma = 6;
        this->shift_alpha = 12;
        this->thresh_mult = 6;
    }

    conf_info_t(
        /* <<--ctor-args-->> */
        int32_t num_loads,
        int32_t batches_perload, 
        int32_t tsamps_perbatch, 
        int32_t num_windows, 
        int32_t elecs_perwin,
        int32_t hiddens_perwin, 
        bool do_init,
        int32_t thresh_batches,
        int32_t backprop_batches,
        int32_t shift_thresh,
        int32_t shift_tsamps,
        int32_t shift_elecs,
        int32_t shift_gamma,
        int32_t shift_alpha,
        int32_t thresh_mult
        )
    {
        /* <<--ctor-custom-->> */
        this->num_loads = num_loads;
        this->batches_perload = batches_perload;
        this->tsamps_perbatch = tsamps_perbatch;
        this->num_windows = num_windows;
        this->elecs_perwin = elecs_perwin;
        this->hiddens_perwin = hiddens_perwin;
        this->do_init = do_init;
        this->thresh_batches = thresh_batches;
        this->backprop_batches = backprop_batches;
        this->shift_thresh = shift_thresh;
        this->shift_tsamps = shift_tsamps;
        this->shift_elecs = shift_elecs;
        this->shift_gamma = shift_gamma;
        this->shift_alpha = shift_alpha;
        this->thresh_mult = thresh_mult;
    }

    // equals operator
    inline bool operator==(const conf_info_t &rhs) const
    {
        /* <<--eq-->> */
        if (num_loads != rhs.num_loads) return false;
        if (batches_perload != rhs.batches_perload) return false;
        if (tsamps_perbatch != rhs.tsamps_perbatch) return false;
        if (num_windows != rhs.num_windows) return false;
        if (elecs_perwin != rhs.elecs_perwin) return false;
        if (hiddens_perwin != rhs.hiddens_perwin) return false;
        if (do_init != rhs.do_init) return false;
        if (thresh_batches != rhs.thresh_batches) return false;
        if (backprop_batches != rhs.backprop_batches) return false;
        if (shift_thresh != rhs.shift_thresh) return false;
        if (shift_tsamps != rhs.shift_tsamps) return false;
        if (shift_elecs != rhs.shift_elecs) return false;
        if (shift_gamma != rhs.shift_gamma) return false;
        if (shift_alpha != rhs.shift_alpha) return false;
        if (thresh_mult != rhs.thresh_mult) return false;
        return true;
    }

    // assignment operator
    inline conf_info_t& operator=(const conf_info_t& other)
    {
        /* <<--assign-->> */
        num_loads = other.num_loads;
        batches_perload = other.batches_perload;
        tsamps_perbatch = other.tsamps_perbatch;
        num_windows = other.num_windows;
        elecs_perwin = other.elecs_perwin;
        hiddens_perwin = other.hiddens_perwin;
        do_init = other.do_init;
        shift_thresh = other.shift_thresh;
        shift_tsamps = other.shift_tsamps;
        shift_elecs = other.shift_elecs;
        shift_gamma = other.shift_gamma;
        shift_alpha = other.shift_alpha;
        thresh_mult = other.thresh_mult;
        return *this;
    }

    // VCD dumping function
    friend void sc_trace(sc_trace_file *tf, const conf_info_t &v, const std::string &NAME)
    {}

    // redirection operator
    friend ostream& operator << (ostream& os, conf_info_t const &conf_info)
    {
        os << "{";
        /* <<--print-->> */
        os << "num_loads = " << conf_info.num_loads << "";
        os << "batches_perload = " << conf_info.batches_perload << ", ";
        os << "tsamps_perbatch = " << conf_info.tsamps_perbatch << ", ";
        os << "num_windows = " << conf_info.num_windows << ", ";
        os << "elecs_perwin = " << conf_info.elecs_perwin << ", ";
        os << "hiddens_perwin = " << conf_info.hiddens_perwin << ", ";
        os << "do_init = " << conf_info.do_init << "";
        os << "shift_thresh = " << a_read(conf_info.shift_thresh) << ", ";
        os << "shift_tsamps = " << conf_info.shift_tsamps << ", ";
        os << "shift_elecs = " << a_read(conf_info.shift_elecs) << "";
        os << "shift_gamma = " << a_read(conf_info.shift_gamma) << "";
        os << "shift_alpha = " << conf_info.shift_alpha << "";
        os << "thresh_mult = " << conf_info.thresh_mult << "";
        os << "}";
        return os;
    }

        /* <<--params-->> */
        int32_t num_loads;
        int32_t batches_perload;
        int32_t tsamps_perbatch;
        int32_t num_windows;
        int32_t elecs_perwin;
        int32_t hiddens_perwin;
        bool do_init;
        int32_t shift_thresh;
        int32_t shift_tsamps;
        int32_t shift_elecs;
        int32_t shift_gamma;
        bool shift_alpha;
        int32_t thresh_mult;
};

#endif // __MINDFUZZ_CONF_INFO_HPP__

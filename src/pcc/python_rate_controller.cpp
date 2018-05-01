#ifdef QUIC_PORT
#ifdef QUIC_PORT_LOCAL
#include "net/quic/core/congestion_control/pcc_sender.h"
#include "net/quic/core/congestion_control/rtt_stats.h"
#include "net/quic/core/quic_time.h"
#include "net/quic/platform/api/quic_str_cat.h"
#else
#include "third_party/pcc_quic/pcc_sender.h"
#include "/quic/src/core/congestion_control/rtt_stats.h"
#include "/quic/src/net/platform/api/quic_str_cat.h"
#include "base_commandlineflags.h"
#endif
#else
#include "python_rate_controller.h"
#endif

#include <algorithm>

#ifdef QUIC_PORT
#ifdef QUIC_PORT_LOCAL
namespace net {

#else
namespace gfe_quic {
#endif
#endif

PccPythonRateController::PccPythonRateController() {
    Py_Initialize();
    PyRun_SimpleString("import sys");
   
    last_given_mi_id = -1;

    std::stringstream set_argv_ss;
    set_argv_ss << "sys.argv = [";
    wchar_t** unicode_args = new wchar_t*[Options::argc];
    for (int i = 0; i < Options::argc; ++i) {
        const char* arg = Options::argv[i];
        if (i == 0) {
            set_argv_ss << "\"" << arg << "\"";
        } else {
            set_argv_ss << ", \"" << arg << "\"";
        }
        int len = strlen(arg);
        std::wstring wc(len, L'#' );
        mbstowcs(&wc[0], arg, len);
        unicode_args[i] = &wc[0];
    }
    set_argv_ss << "]";
    std::string set_argv_str = set_argv_ss.str();
    PyRun_SimpleString(set_argv_str.c_str());
    
    const char* python_path_arg = Options::Get("-pypath="); // The location in which the pcc_addon.py file can be found.
    if (python_path_arg != NULL) {
        int python_path_arg_len = strlen(python_path_arg);
        char python_path_cmd_buf[python_path_arg_len + 50];
        sprintf(&python_path_cmd_buf[0], "sys.path.append(\"%s\")", python_path_arg);
        PyRun_SimpleString(&python_path_cmd_buf[0]);
    }

    const char* python_filename = "pcc_rate_controller.py";
    const char* python_filename_arg = Options::Get("-pyhelper=");
    if (python_filename_arg != NULL) {
        python_filename = python_filename_arg;
    }
    
    module = PyImport_ImportModule(python_filename);
    if (module == NULL) {
        std::cerr << "ERROR: Could not load python module: " << python_filename << std::endl;
        PyErr_Print();
        exit(-1);
    }
    //Py_DECREF(module_name);
    give_sample_func = PyObject_GetAttrString(module, "give_sample");
    if (give_sample_func == NULL) {
        std::cerr << "ERROR: Could not load python function: give_sample" << std::endl;
        PyErr_Print();
        exit(-1);
    }
    get_rate_func = PyObject_GetAttrString(module, "get_rate");
    if (get_rate_func == NULL) {
        std::cerr << "ERROR: Could not load python function: get_rate" << std::endl;
        PyErr_Print();
        exit(-1);
    }
    reset_func = PyObject_GetAttrString(module, "reset");
    if (reset_func == NULL) {
        std::cerr << "ERROR: Could not load python function: reset" << std::endl;
        PyErr_Print();
        exit(-1);
    }
}

void PccPythonRateController::Reset() {
    PyObject* result = PyObject_CallObject(reset_func, NULL);
    PyErr_Print();
}

void PccPythonRateController::GiveSample(double rate, double recv_rate, double lat, double loss, double lat_infl, double utility) {
    PyObject* args = PyTuple_New(6);
    PyObject* sending_rate_value = PyFloat_FromDouble(rate);
    PyObject* recv_rate_value = PyFloat_FromDouble(recv_rate);
    PyObject* latency_value = PyFloat_FromDouble(lat);
    PyObject* loss_rate_value = PyFloat_FromDouble(loss);
    PyObject* latency_inflation_value = PyFloat_FromDouble(lat_infl);
    PyObject* utility_value = PyFloat_FromDouble(utility);
    PyTuple_SetItem(args, 0, sending_rate_value);
    PyTuple_SetItem(args, 1, recv_rate_value);
    PyTuple_SetItem(args, 2, latency_value);
    PyTuple_SetItem(args, 3, loss_rate_value);
    PyTuple_SetItem(args, 4, latency_inflation_value);
    PyTuple_SetItem(args, 5, utility_value);
    PyObject_CallObject(give_sample_func, args);
}

void PccPythonRateController::GiveMiSample(MonitorInterval& mi) {
    double sending_rate = mi.GetTargetSendingRate();
    double recv_rate = mi.GetObsThroughput();
    double latency = mi.GetObsRtt();
    double loss_rate = mi.GetObsLossRate();
    double latency_inflation = mi.GetObsRttInflation();
    double utility = mi.GetObsUtility();
    GiveSample(sending_rate, recv_rate, latency, loss_rate, latency_inflation, utility);
}

QuicBandwidth PccPythonRateController::GetNextSendingRate(
        PccMonitorIntervalAnalysisGroup& past_monitor_intervals,
        QuicBandwidth current_rate,
        QuicTime cur_time) {

    if (past_monitor_intervals.Empty()) {
        GiveSample(0, 0, 0, 0, 0, 0);
    } else {
        std::deque<MonitorInterval>::iterator it = past_monitor_intervals.Begin();
        
        while (it != past_monitor_intervals.End() && it->GetId() <= last_given_mi_id) {
            ++it;
        }

        while (it != past_monitor_intervals.End()) {
            GiveMiSample(*it);
            last_given_mi_id = it->GetId();
            ++it;
        }
    }

    PyObject* result = PyObject_CallObject(get_rate_func, NULL);
    if (result == NULL) {
        std::cout << "ERROR: Failed to call python get_rate() func" << std::endl;
        PyErr_Print();
        exit(-1);
    }
    double result_double = PyFloat_AsDouble(result);
    PyErr_Print();
    if (!PyFloat_Check(result)) {
        std::cout << "ERROR: Output from python get_rate() is not a float" << std::endl;
        exit(-1);
    }
    return result_double;
}

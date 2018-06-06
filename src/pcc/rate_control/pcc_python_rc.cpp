
#include "pcc_python_rc.h"
#include <algorithm>

PccPythonRateController::PccPythonRateController(double call_freq,
        PccEventLogger* log) {
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
        //std::cerr << "Adding python path: " << python_path_arg << std::endl;
    }

    const char* python_filename = "pcc_rate_controller";
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
    //std::cerr << "Creating arguments to give sample" << std::endl;
    if (PyErr_Occurred()) {
        std::cerr << "Python error occurred" << std::endl;
    }
    static PyObject* args = PyTuple_New(6);
    PyObject* sending_rate_value = PyFloat_FromDouble(rate);
    PyObject* recv_rate_value = PyFloat_FromDouble(recv_rate);
    PyObject* latency_value = PyFloat_FromDouble(lat);
    PyObject* loss_rate_value = PyFloat_FromDouble(loss);
    PyObject* latency_inflation_value = PyFloat_FromDouble(lat_infl);
    PyObject* utility_value = PyFloat_FromDouble(utility);
    //std::cerr << "Assembling list" << std::endl;
    if (PyErr_Occurred()) {
        std::cerr << "Python error occurred" << std::endl;
    }
    PyTuple_SetItem(args, 0, sending_rate_value);
    PyTuple_SetItem(args, 1, recv_rate_value);
    PyTuple_SetItem(args, 2, latency_value);
    PyTuple_SetItem(args, 3, loss_rate_value);
    PyTuple_SetItem(args, 4, latency_inflation_value);
    PyTuple_SetItem(args, 5, utility_value);
    //std::cerr << "Calling give_sample_func" << std::endl;
    if (PyErr_Occurred()) {
        std::cerr << "Python error occurred" << std::endl;
    }
    PyObject_CallObject(give_sample_func, args);
    //std::cerr << "Done giving sample" << std::endl;
    if (PyErr_Occurred()) {
        std::cerr << "Python error occurred" << std::endl;
    }
    //std::cerr << "GiveSample finished" << std::endl;
    //Py_DECREF(args);
    //Py_DECREF(sending_rate_value);
    //Py_DECREF(recv_rate_value);
    //Py_DECREF(latency_value);
    //Py_DECREF(loss_rate_value);
    //Py_DECREF(latency_inflation_value);
    //Py_DECREF(utility_value);
}

void PccPythonRateController::GiveMiSample(const MonitorInterval& mi) {
    double sending_rate = mi.GetTargetSendingRate();
    double recv_rate = mi.GetObsThroughput();
    double latency = mi.GetObsRtt();
    double loss_rate = mi.GetObsLossRate();
    double latency_inflation = mi.GetObsRttInflation();
    double utility = mi.GetObsUtility();
    GiveSample(sending_rate, recv_rate, latency, loss_rate, latency_inflation, utility);
}

void PccPythonRateController::MonitorIntervalFinished(const MonitorInterval& mi) {
    //mi_cache.push_back(MonitorInterval(mi));
    GiveMiSample(mi);
}

QuicBandwidth PccPythonRateController::GetNextSendingRate( QuicBandwidth current_rate, QuicTime cur_time) {

    for (int i = 0; i < mi_cache.size(); ++i) {
        GiveMiSample(mi_cache[i]);
    }
    mi_cache.clear();

    //std::cerr << "Calling get_rate_func" << std::endl;
    if (PyErr_Occurred()) {
        std::cerr << "Python error occurred" << std::endl;
    }
    PyObject* result = PyObject_CallObject(get_rate_func, NULL);
    if (result == NULL) {
        std::cout << "ERROR: Failed to call python get_rate() func" << std::endl;
        PyErr_Print();
        exit(-1);
    }
    //std::cerr << "Converting result to double" << std::endl;
    if (PyErr_Occurred()) {
        std::cerr << "Python error occurred" << std::endl;
    }
    double result_double = PyFloat_AsDouble(result);
    PyErr_Print();
    if (!PyFloat_Check(result)) {
        std::cout << "ERROR: Output from python get_rate() is not a float" << std::endl;
        exit(-1);
    }
    //std::cerr << "Returning new rate" << std::endl;
    if (PyErr_Occurred()) {
        std::cerr << "Python error occurred" << std::endl;
    }
    //std::cerr << "GetNextSendingRate finished" << std::endl;
    return result_double;
}

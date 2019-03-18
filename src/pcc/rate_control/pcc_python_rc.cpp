
#include "pcc_python_rc.h"
#include <algorithm>

std::mutex PccPythonRateController::interpreter_lock_;
bool PccPythonRateController::python_initialized_ = false;

void PccPythonRateController::InitializePython() {
    Py_Initialize();
    PyRun_SimpleString("import sys");

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

    python_initialized_ = true;
}

int PccPythonRateController::GetNextId() {
    static int next_id = 0;
    int id = next_id;
    ++next_id;
    return id;
}

PccPythonRateController::PccPythonRateController(double call_freq,
        PccEventLogger* log) {

    std::lock_guard<std::mutex> lock(interpreter_lock_);
    if (!python_initialized_) {
        InitializePython();
    }
    
    id = GetNextId();

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
    
    PyObject* init_func = PyObject_GetAttrString(module, "init");
    if (init_func == NULL) {
        std::cerr << "ERROR: Could not load python function: init" << std::endl;
        PyErr_Print();
        exit(-1);
    }
    PyObject* id_obj = PyLong_FromLong(id);
    static PyObject* args = PyTuple_New(1);
    PyTuple_SetItem(args, 0, id_obj);
    
    PyObject* init_result = PyObject_CallObject(init_func, args);
    PyErr_Print();
    
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
    std::cout << "Starting Reset" << std::endl;
    std::lock_guard<std::mutex> lock(interpreter_lock_);
    PyObject* id_obj = PyLong_FromLong(id);
    static PyObject* args = PyTuple_New(1);
    PyTuple_SetItem(args, 0, id_obj);
    
    PyObject* result = PyObject_CallObject(reset_func, args);
    PyErr_Print();
}

void PccPythonRateController::GiveSample(long id,
                                         long bytes_sent,
                                         long bytes_acked,
                                         long bytes_lost,
                                         double send_start_time,
                                         double send_end_time,
                                         double recv_start_time,
                                         double recv_end_time,
                                         std::vector<double> rtt_samples,
                                         long packet_size,
                                         double utility) {
    
    std::lock_guard<std::mutex> lock(interpreter_lock_);
    static PyObject* args = PyTuple_New(11);
    PyObject* id_obj = PyLong_FromLong(id);
    PyObject* bytes_sent_obj = PyLong_FromLong(bytes_sent);
    PyObject* bytes_acked_obj = PyLong_FromLong(bytes_acked);
    PyObject* bytes_lost_obj = PyLong_FromLong(bytes_lost);
    PyObject* send_start_obj = PyFloat_FromDouble(send_start_time);
    PyObject* send_end_obj = PyFloat_FromDouble(send_end_time);
    PyObject* recv_start_obj = PyFloat_FromDouble(recv_start_time);
    PyObject* recv_end_obj = PyFloat_FromDouble(recv_end_time);

    PyObject *rtt_samples_obj = PyList_New(rtt_samples.size());
    for (size_t i = 0; i < rtt_samples.size(); ++i){
        PyList_SetItem(rtt_samples_obj, i, PyFloat_FromDouble(rtt_samples[i]));
    } 

    PyObject* packet_size_obj = PyLong_FromLong(packet_size);
    PyObject* utility_obj = PyFloat_FromDouble(utility);
    
    PyTuple_SetItem(args, 0, id_obj);
    PyTuple_SetItem(args, 1, bytes_sent_obj);
    PyTuple_SetItem(args, 2, bytes_acked_obj);
    PyTuple_SetItem(args, 3, bytes_lost_obj);
    PyTuple_SetItem(args, 4, send_start_obj);
    PyTuple_SetItem(args, 5, send_end_obj);
    PyTuple_SetItem(args, 6, recv_start_obj);
    PyTuple_SetItem(args, 7, recv_end_obj);
    PyTuple_SetItem(args, 8, rtt_samples_obj);
    PyTuple_SetItem(args, 9, packet_size_obj);
    PyTuple_SetItem(args, 10, utility_obj);
    
    PyObject_CallObject(give_sample_func, args);
}

void PccPythonRateController::GiveMiSample(const MonitorInterval& mi) {
    long id = mi.GetId();
    long bytes_sent = mi.GetBytesSent();
    long bytes_acked = mi.GetBytesAcked();
    long bytes_lost = mi.GetBytesLost();
    double send_start_time = mi.GetSendStartTime() / 1e6;
    double send_end_time = mi.GetSendEndTime() / 1e6;
    double recv_start_time = mi.GetRecvStartTime() / 1e6;
    double recv_end_time = mi.GetRecvEndTime() / 1e6;
    std::vector<double> rtt_samples;
    mi.GetRttSamples(&rtt_samples);
    long packet_size = mi.GetPacketSize();
    double utility = mi.GetUtility();
    GiveSample(0, bytes_sent, bytes_acked, bytes_lost, send_start_time,
               send_end_time, recv_start_time, recv_end_time, rtt_samples,
               packet_size, utility);
}

void PccPythonRateController::MonitorIntervalFinished(const MonitorInterval& mi) {
    GiveMiSample(mi);
}

QuicBandwidth PccPythonRateController::GetNextSendingRate(QuicBandwidth current_rate, QuicTime cur_time) {

    std::lock_guard<std::mutex> lock(interpreter_lock_);
    
    PyObject* id_obj = PyLong_FromLong(id);
    static PyObject* args = PyTuple_New(1);
    PyTuple_SetItem(args, 0, id_obj);
    
    PyObject* result = PyObject_CallObject(get_rate_func, args);
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
    Py_DECREF(result);

    return result_double;
}

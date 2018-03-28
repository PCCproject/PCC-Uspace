
#include "pcc_python_helper.h"

PccPythonHelper::PccPythonHelper(const std::string& python_filename) {
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
    
    const char* python_path_arg = Options::Get("-pypath="); // The location in which the pcc_addon.py file can be found.
    if (python_path_arg != NULL) {
        int python_path_arg_len = strlen(python_path_arg);
        char python_path_cmd_buf[python_path_arg_len + 50];
        sprintf(&python_path_cmd_buf[0], "sys.path.append(\"%s\")", python_path_arg);
        PyRun_SimpleString(&python_path_cmd_buf[0]);
    }
    module = PyImport_ImportModule(python_filename.c_str());
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
}

PccPythonHelper::~PccPythonHelper() {
    PyObject* save_model_func = PyObject_GetAttrString(module, "save_model");
    if (save_model_func == NULL) {
        std::cerr << "ERROR: Could not load python function: save_model" << std::endl;
        PyErr_Print();
        exit(-1);
    }
    PyObject_CallObject(save_model_func, NULL);
    Py_DECREF(save_model_func);
    Py_DECREF(give_sample_func);
    Py_DECREF(get_rate_func);
    Py_DECREF(module);
    Py_Finalize();
}

void PccPythonHelper::GiveSample(double sending_rate, double latency, double loss_rate, double latency_inflation, double utility) {
    PyObject* args = PyTuple_New(5);
    PyObject* sending_rate_value = PyFloat_FromDouble(sending_rate);
    PyObject* latency_value = PyFloat_FromDouble(latency);
    PyObject* loss_rate_value = PyFloat_FromDouble(loss_rate);
    PyObject* latency_inflation_value = PyFloat_FromDouble(latency_inflation);
    PyObject* utility_value = PyFloat_FromDouble(utility);
    PyTuple_SetItem(args, 0, sending_rate_value);
    PyTuple_SetItem(args, 1, latency_value);
    PyTuple_SetItem(args, 2, loss_rate_value);
    PyTuple_SetItem(args, 3, latency_inflation_value);
    PyTuple_SetItem(args, 4, utility_value);
    PyObject_CallObject(give_sample_func, args);
    //Py_DECREF(sending_rate_value);
    //Py_DECREF(latency_value);
    //Py_DECREF(loss_rate_value);
    //Py_DECREF(latency_inflation_value);
    //Py_DECREF(utility_value);
    // Py_DECREF(args); // This line causes segfaults for some reason. I'm not sure why, but it has something to do with
    // reference counting in the python interpreter.
    //Py_DECREF(result);
}

double PccPythonHelper::GetRate() {
    PyObject* result = PyObject_CallObject(get_rate_func, NULL);
    double result_double = PyFloat_AS_DOUBLE(result);
    //Py_DECREF(result);
    return result_double;
}

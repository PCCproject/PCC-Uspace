#ifndef _UDT_TEST_UTIL_H_
#define _UDT_TEST_UTIL_H_

struct UDTUpDown {
  // use this function to initialize the UDT library
  UDTUpDown() {UDT::startup();}
  // use this function to release the UDT library
  ~UDTUpDown() {UDT::cleanup();}
};

#endif

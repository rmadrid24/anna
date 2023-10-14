//
//  generator.h
//  YCSB-C
//
//  Created by Jinglei Ren on 12/6/14.
//  Copyright (c) 2014 Jinglei Ren <jinglei@ren.systems>.
//

#ifndef ANNA_GENERATOR_H_
#define ANNA_GENERATOR_H_

#include <cstdint>
#include <string>

namespace benchmark {

template <typename Value>
class Generator {
 public:
  virtual Value Next() = 0;
  virtual Value Last() = 0;
  virtual ~Generator() { }
};

}

#endif // ANNA_GENERATOR_H_

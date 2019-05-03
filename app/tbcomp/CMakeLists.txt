set(src_tbcomp
                    bitarray.hpp
  bitbuffer.cpp     bitbuffer.hpp
  decisiontree.cpp  decisiontree.hpp
  dtnode.cpp        dtnode.hpp
  dtxnode.cpp       dtxnode.hpp
  huffman.cpp       huffman.hpp
  posindex.cpp      posindex.hpp
  predicate.cpp     predicate.hpp
  predicates.cpp    predicates.hpp
  repair.cpp        repair.hpp
  symbolarray.cpp   symbolarray.hpp
                    taskrunner.hpp
  tbcomp.cpp
                    tbutil.hpp
  test.cpp          test.hpp
  wdlcomp.cpp       wdlcomp.hpp
  wdlnode.cpp       wdlnode.hpp
  )

if("${CMAKE_SIZEOF_VOID_P}" EQUAL "8")
  if(NOT ANDROID)
    find_package(Boost 1.66 REQUIRED)
    add_executable(tbcomp ${src_tbcomp})
    target_link_libraries(tbcomp
      PUBLIC texelutillib
      PRIVATE Boost::boost
      )
  endif()
endif()  

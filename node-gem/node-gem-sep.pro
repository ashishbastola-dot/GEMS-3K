TEMPLATE	= app
LANGUAGE        = C++
TARGET		= node-gem
VERSION         = 3.0

CONFIG		-= qt
CONFIG		+=  warn_on debug windows
CONFIG		+= console

DEFINES         += IPMGEMPLUGIN

!win32 {
  DEFINES += __unix
}

#LIBS_CPP       =  ../../vizor/libs
#KERNEL_CPP     =  ../../vizor/kernel
#MODS_CPP       =  ../
#SUBMOD_CPP     =  ../submod
#NUMERICS_CPP     =  ../numerics
#GEMIPM_CPP     =  ../gemipm

LIBS_CPP       =  ../GEMS3K
KERNEL_CPP     =  ../GEMS3K
MODS_CPP       =  ../GEMS3K
SUBMOD_CPP     =  ../GEMS3K
NUMERICS_CPP   =  ../GEMS3K
GEMIPM_CPP     =  ../GEMS3K

KERNEL_H     =  $$KERNEL_CPP
LIBS_H       =  $$LIBS_CPP
MODS_H       =  $$MODS_CPP
SUBMOD_H     =  $$SUBMOD_CPP
NUMERICS_H   =  $$NUMERICS_CPP
GEMIPM_H     =  $$GEMIPM_CPP

DEPENDPATH   +=
DEPENDPATH   += .
DEPENDPATH   += $$GEMIPM_H
DEPENDPATH   += $$KERNEL_H
DEPENDPATH   += $$LIBS_H
DEPENDPATH   += $$MODS_H
DEPENDPATH   += $$SUBMOD_H
DEPENDPATH   += $$NUMERICS_H

INCLUDEPATH   +=
INCLUDEPATH   += .
INCLUDEPATH   += $$GEMIPM_H
INCLUDEPATH   += $$KERNEL_H
INCLUDEPATH   += $$LIBS_H
INCLUDEPATH   += $$MODS_H
INCLUDEPATH   += $$SUBMOD_H
INCLUDEPATH   += $$NUMERICS_H

OBJECTS_DIR       = obj

       HEADERS	 += $$LIBS_H/verror.h  \
                    $$LIBS_H/gdatastream.h  \
                    $$KERNEL_H/v_user.h \
                    $$MODS_H/s_fgl.h \
                    $$SUBMOD_H/databr.h \
                    $$SUBMOD_H/datach.h \
                    $$SUBMOD_H/node.h \
                    $$SUBMOD_H/ms_multi.h \
	                $$SUBMOD_H/io_arrays.h \
		            $$GEMIPM_H/m_const.h \
		            $$GEMIPM_H/m_param.h  \
                    $$NUMERICS_H/num_methods.h \
                    $$NUMERICS_H/tnt_i_refvec.h \
                    $$NUMERICS_H/tnt_array1d.h \
                    $$NUMERICS_H/tnt_array2d.h \
                    $$NUMERICS_H/tnt.h \
                    $$NUMERICS_H/jama_cholesky.h \
                    $$NUMERICS_H/jama_lu.h \
    main.h

        SOURCES	  +=  $$LIBS_CPP/gdatastream.cpp  \
                     $$MODS_CPP/s_fgl.cpp \
                     $$MODS_CPP/s_fgl1.cpp \
                     $$MODS_CPP/s_fgl2.cpp \
                     $$MODS_CPP/s_fgl3.cpp \
                     $$MODS_CPP/s_fgl4.cpp \
                    $$SUBMOD_CPP/node.cpp \
                    $$SUBMOD_CPP/node_format.cpp \
                    $$SUBMOD_CPP/ms_multi_file.cpp \
                    $$SUBMOD_CPP/ms_multi_format.cpp \
                    $$SUBMOD_CPP/ipm_chemical.cpp \
                    $$SUBMOD_CPP/ipm_chemical2.cpp \
                    $$SUBMOD_CPP/ipm_chemical3.cpp \
                    $$SUBMOD_CPP/ipm_main.cpp \
                    $$SUBMOD_CPP/ipm_simplex.cpp \
		            $$SUBMOD_CPP/io_arrays.cpp \
                    main.cpp \
		            $$GEMIPM_CPP/ms_param.cpp \
                    $$NUMERICS_CPP/num_methods.cpp

#include "log.h"
#include <string.h>
#include <time.h>
#include <sys/time.h>

Log::Log(){
    m_count = 0;
    m_is_async = false;
}

Log::~Log(){
    if(m_fp != nullptr){
        fclose(m_fp);
    }
}
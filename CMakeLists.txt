cmake_minimum_required(VERSION 3.23)
project(A2022 C)

set(CMAKE_C_STANDARD 99)

add_executable(A22-solution main.c global_defs.h configuration.c configuration.h
        direct_fork.c direct_fork.h analysis.c analysis.h utility.c utility.h reducers.c reducers.h fifo_processes.c fifo_processes.h mq_processes.c mq_processes.h)

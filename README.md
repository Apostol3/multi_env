# C++-based multiplexer for nlab

Drop-in replacement for [pymulti-env](https://github.com/apostol3/pymulti_env), rewritten in C++.

## Install
### Requirements
* [rapidjson](https://github.com/Tencent/rapidjson)
* [asio](https://github.com/chriskohlhoff/asio) (non-boost version)
* [tiny-process-library](https://github.com/eidheim/tiny-process-library)
* C++17 compliant compiler
### Build
You can find Makefile and VS2017 solution files in repo.
Tested:
* clang 5.0.0
* gcc 7.2.0
* Visual Studio 2017

## Usage
````
Usage: ../multi_env/multi_env [OPTIONS] count command

Positionals:
  count INT (REQUIRED)        count of environments to start
  command TEXT (REQUIRED)     command to execute environments

Options:
  -h,--help                   Print this help message and exit
  -I,--envs-uri TEXT=tcp://127.0.0.1:15005
                              enviroments URI in format 'tcp://hostname:port'
  -O,--nlab-uri TEXT=tcp://127.0.0.1:15005
                              nlab URI in format 'tcp://hostname:port'
  -e,--existing               do not spawn enviroments, just connect to them
````
multi_env:
	g++ --std=c++14 \
	main.cpp nlab.cpp remote_env.cpp \
	-pthread -O3 \
	-I CLI11/include \
	-L tiny-process-library/ \
	-ltiny-process-library \
	-o multi_env

clean:
	rm multi_env

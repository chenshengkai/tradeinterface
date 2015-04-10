#g++ -lthosttraderapi -lglog log.cpp config.cpp trader.cpp -o trader
g++ -lthosttraderapi -lpthread log4z.cpp log.cpp config.cpp trader.cpp -o trader

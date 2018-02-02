/*
    TTL 7474	    
    
	Dual D−Type Positive−Edge−Triggered
	Flip−Flop w/Preset and Clear

                _____   _____
     /1CLR   1 |*    \_/     | 14  Vcc
        1D   2 |             | 13  /2CLR
      1CLK   3 |             | 12  2D
     /1PRE   4 |             | 11  2CLK
        1Q   5 |             | 10  /2PRE
       /1Q   6 |             | 9   2Q
       GND   7 |_____________| 8   /2Q
*/

module ttl_7474(
    input _1CLR_N, 
	input _1D,
    input _1CLK, 
	input _1PRE_N,
    output reg _1Q, 
	output _1Q_N,
    input _2CLR_N, 
	input _2D,
    input _2CLK, 
	input _2PRE_N,
    output _2Q, 
	output _2Q_N
);
	assign _1Q_N = ~_1Q;
	assign _2Q_N = ~_2Q;
     
    //flip flop state is affected irrespective of the clock 
    always @(posedge(_1CLK) or negedge(_1CLR_N) or negedge(_1PRE_N))
    begin
		if(_1CLR_N == 0)
            _1Q_N = 0;
        else if(_1PRE_N == 0)
			_1Q_N = 1; 
		else if (_1CLK == 1)
			_1Q_N = _1D;             
    end 
	    always @(posedge(_2CLK) or negedge(_2CLR_N) or negedge(_2PRE_N))
    begin
		if(_2CLR_N == 0)
            _2Q_N = 0;
        else if(_2PRE_N == 0)
			_2Q_N = 1; 
		else if (_2CLK == 1)
			_2Q_N = _2D;             
    end 
endmodule
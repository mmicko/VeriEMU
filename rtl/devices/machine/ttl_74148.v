/*
    TTL 74148	    
    
*/

module ttl_74148(
    input EI,
	input [7:0] I,
    output reg [2:0] A, 
	output reg GS,
    output reg EO
);
    always @*
    begin
        if (EI==1'b0) 
        begin
          if (I==8'b11111111)
          begin
            A = 3'b111;
            GS = 1'b1;
            EO = 1'b0;
          end
          else
          begin
            GS = 1'b0;
            EO = 1'b1;
            if (I[7:0]==8'b01111111)
                A = 3'b111;
            else 
            if (I[6:0]==7'b0111111)
                A = 3'b110;
            else 
            if (I[5:0]==6'b011111)
                A = 3'b101;
            else 
            if (I[4:0]==5'b01111)
                A = 3'b100;
            else 
            if (I[3:0]==4'b0111)
                A = 3'b011;
            else 
            if (I[2:0]==3'b011)
                A = 3'b010;
            else 
            if (I[1:0]==2'b01)
                A = 3'b001;
            else 
                A = 3'b000;
          end
        end
        else
        begin
            A = 3'b111;
            GS = 1'b1;
            EO = 1'b1;
        end
    end
endmodule
//----------------------------------------------------------------------------
// user_logic.vhd - module
//----------------------------------------------------------------------------
//
// ***************************************************************************
// ** Copyright (c) 1995-2008 Xilinx, Inc.  All rights reserved.            **
// **                                                                       **
// ** Xilinx, Inc.                                                          **
// ** XILINX IS PROVIDING THIS DESIGN, CODE, OR INFORMATION "AS IS"         **
// ** AS A COURTESY TO YOU, SOLELY FOR USE IN DEVELOPING PROGRAMS AND       **
// ** SOLUTIONS FOR XILINX DEVICES.  BY PROVIDING THIS DESIGN, CODE,        **
// ** OR INFORMATION AS ONE POSSIBLE IMPLEMENTATION OF THIS FEATURE,        **
// ** APPLICATION OR STANDARD, XILINX IS MAKING NO REPRESENTATION           **
// ** THAT THIS IMPLEMENTATION IS FREE FROM ANY CLAIMS OF INFRINGEMENT,     **
// ** AND YOU ARE RESPONSIBLE FOR OBTAINING ANY RIGHTS YOU MAY REQUIRE      **
// ** FOR YOUR IMPLEMENTATION.  XILINX EXPRESSLY DISCLAIMS ANY              **
// ** WARRANTY WHATSOEVER WITH RESPECT TO THE ADEQUACY OF THE               **
// ** IMPLEMENTATION, INCLUDING BUT NOT LIMITED TO ANY WARRANTIES OR        **
// ** REPRESENTATIONS THAT THIS IMPLEMENTATION IS FREE FROM CLAIMS OF       **
// ** INFRINGEMENT, IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS       **
// ** FOR A PARTICULAR PURPOSE.                                             **
// **                                                                       **
// ***************************************************************************
//
//----------------------------------------------------------------------------
// Filename:          user_logic.vhd
// Version:           1.00.a
// Description:       User logic module.
// Date:              Fri Oct 18 19:27:50 2013 (by Create and Import Peripheral Wizard)
// Verilog Standard:  Verilog-2001
//----------------------------------------------------------------------------
// Naming Conventions:
//   active low signals:                    "*_n"
//   clock signals:                         "clk", "clk_div#", "clk_#x"
//   reset signals:                         "rst", "rst_n"
//   generics:                              "C_*"
//   user defined types:                    "*_TYPE"
//   state machine next state:              "*_ns"
//   state machine current state:           "*_cs"
//   combinatorial signals:                 "*_com"
//   pipelined or register delay signals:   "*_d#"
//   counter signals:                       "*cnt*"
//   clock enable signals:                  "*_ce"
//   internal version00000000000000000000000000000000 of output port:       "*_i"
//   device pins:                           "*_pin"
//   ports:                                 "- Names begin with Uppercase"
//   processes:                             "*_PROCESS"
//   component instantiations:              "<ENTITY_>I_<#|FUNC>"
//----------------------------------------------------------------------------

module user_logic
(
  // -- ADD USER PORTS BELOW THIS LINE ---------------
  IR_signal,
  Interrupt,
  // --USER ports added here 
  // -- ADD USER PORTS ABOVE THIS LINE ---------------

  // -- DO NOT EDIT BELOW THIS LINE ------------------
  // -- Bus protocol ports, do not add to or delete 
  Bus2IP_Clk,                     // Bus to IP clock
  Bus2IP_Reset,                   // Bus to IP reset
  Bus2IP_Data,                    // Bus to IP data bus
  Bus2IP_BE,                      // Bus to IP byte enables
  Bus2IP_RdCE,                    // Bus to IP read chip enable
  Bus2IP_WrCE,                    // Bus to IP write chip enable
  IP2Bus_Data,                    // IP to Bus data bus
  IP2Bus_RdAck,                   // IP to Bus read transfer acknowledgement
  IP2Bus_WrAck,                   // IP to Bus write transfer acknowledgement
  IP2Bus_Error                    // IP to Bus error response
  // -- DO NOT EDIT ABOVE THIS LINE ------------------
); // user_logic

// -- ADD USER PARAMETERS BELOW THIS LINE ------------
// --USER parameters added here 
// -- ADD USER PARAMETERS ABOVE THIS LINE ------------

// -- DO NOT EDIT BELOW THIS LINE --------------------
// -- Bus protocol parameters, do not add to or delete
parameter C_SLV_DWIDTH                   = 32;
parameter C_NUM_REG                      = 3;
// -- DO NOT EDIT ABOVE THIS LINE --------------------

// -- ADD USER PORTS BELOW THIS LINE -----------------
// --USER ports added here 
input IR_signal;
output reg Interrupt;
// -- ADD USER PORTS ABOVE THIS LINE -----------------

// -- DO NOT EDIT BELOW THIS LINE --------------------
// -- Bus protocol ports, do not add to or delete
input                                     Bus2IP_Clk;
input                                     Bus2IP_Reset;
input      [0 : C_SLV_DWIDTH-1]           Bus2IP_Data;
input      [0 : C_SLV_DWIDTH/8-1]         Bus2IP_BE;
input      [0 : C_NUM_REG-1]              Bus2IP_RdCE;
input      [0 : C_NUM_REG-1]              Bus2IP_WrCE;
output     [0 : C_SLV_DWIDTH-1]           IP2Bus_Data;
output                                    IP2Bus_RdAck;
output                                    IP2Bus_WrAck;
output                                    IP2Bus_Error;
// -- DO NOT EDIT ABOVE THIS LINE --------------------

//----------------------------------------------------------------------------
// Implementation
//----------------------------------------------------------------------------

  // --USER nets declarations added here, as needed for user logic

  // Nets for user logic slave model s/w accessible register example
  reg        [0 : C_SLV_DWIDTH-1]           slv_reg0;
  reg        [0 : C_SLV_DWIDTH-1]           slv_reg1;
  reg        [0 : C_SLV_DWIDTH-1]           slv_reg2;
  wire       [0 : 2]                        slv_reg_write_sel;
  wire       [0 : 2]                        slv_reg_read_sel;
  reg        [0 : C_SLV_DWIDTH-1]           slv_ip2bus_data;
  wire                                      slv_read_ack;
  wire                                      slv_write_ack;
  integer                                   byte_index, bit_index;

  // --USER logic implementation added here
//Counter variable to determine type of signal
	reg [0:17] decode_counter;
//Temporary register to store the decoded message
  reg        [0 : C_SLV_DWIDTH-1]           temp_reg;
//To determine the end of the incoming message
	reg [0:3] decode_shift;
//Previous value of IR signal
	reg sample_IR_signal;
//Determine if start pulse determined
	reg start_bit_received;
//Start the counter when this is 1
	reg decode;
//negedge or posedge of IR_signal
	reg rise_edge_detected;
	reg fall_edge_detected;
//Reset the interrupt when it is high
	reg interruptreset;

//Initial values
  initial
  begin
	start_bit_received = 1'b0;
	slv_reg0 = 0;
	slv_reg1 = 0;
	slv_reg2 = 0;
	temp_reg = 0;
	Interrupt = 0;
	failedmessage = 0;
  end		

	always @(posedge Bus2IP_Clk)
	begin	       
	
	if ( Bus2IP_Reset == 1 )
        begin
          slv_reg0 = 0;
          slv_reg1 = 0;
          //slv_reg2 = 0;
        end
	else
	begin
	//Reset the interrupt(MSB) and interrupt reset(LSB) when interruptreset is high
		if(slv_reg2[C_SLV_DWIDTH-1] == 1 && (decode_shift >= 1 || decode_shift <= 11))
			begin				
				Interrupt = 0;
				interruptreset = 0;
			end
	
//Pos edge detected when previous value of IR_signal(temp_signal) is 0 and present value of IR_signal is 1
	
		rise_edge_detected = ~sample_IR_signal & IR_signal;
//Neg edge detected when previous value of IR_signal(temp_signal) is 1 and present value of IR_signal is 0

		fall_edge_detected = sample_IR_signal & ~IR_signal;
		
		if(rise_edge_detected)
		begin
			decode = 1'b0;
			//Interrupt = ~Interrupt;
			if(start_bit_received)
			begin
//If counter is around 45000 and Start pulse is already detected, the bit 0 was transmitted. + or - 1000 is margin

				if(decode_counter > 44000 && decode_counter < 46000)
				begin
					temp_reg[31-decode_shift] = 1'b0;
					decode_shift = decode_shift-1;					//0 STATE 
				end
//If counter is around 2 *45000 and Start pulse is already detected, the bit 1 was transmitted. + or - 1000 is margin

				else if(decode_counter > 89000 && decode_counter < 91000)
				begin
					temp_reg[31-decode_shift] = 1'b1;
					decode_shift = decode_shift-1;					//1 STATE
				end
			//When number of pulses after start becomes 12, we have fully decoded the message.
				
				if(decode_shift == 0)
				begin
					start_bit_received = 1'b0;
//Populate decoded message to slv_reg0 from temporary register only when interrupt is not set to HIGH
					if(Interrupt == 0 && interruptreset == 0)
					begin
						Interrupt = 1;									
						slv_reg0 = temp_reg;
						slv_reg1 = slv_reg1 + 1;
					end
					//failedmessage = 0;
					
				end
			end
	//If counter is around 4 *45000, Start pulse is found.
		
			if(decode_counter > 140000)
			begin
				start_bit_received = 1'b1;					//START STATE
				decode_shift = 4'b1011;
				temp_reg = 0;
			end
			
			
		end
	//Enable counter		
		if(fall_edge_detected)
		begin			
			decode = 1'b1;
			decode_counter = 0;
		end
	//When counter is enabled, increment the counter
		
		if(decode)
		begin
			decode_counter = decode_counter + 1;
		end
		
			
		sample_IR_signal = IR_signal;
  end
end
  // ------------------------------------------------------
  // Example code to read/write user logic slave model s/w accessible registers
  // 
  // Note:if ( Bus2IP_Reset == 1 )
  //      begin
  //        slv_reg0 <= 0;
  //       slv_reg1 <= 0;
  //        slv_reg2 <= 0;
  //      end
  // The example code presented here is to show you one way of reading/writing
  // software accessible registers implemented in the user logic slave model.
  // Each bit of the Bus2IP_WrCE/Bus2IP_RdCE signals is configured to correspond
  // to one software accessible register by the top level template. For example,
  // if you have four 32 bit software accessible registers in the user logic,
  // you are basically operating on the following memory mapped registers:
  // 
  //    Bus2IP_WrCE/Bus2IP_RdCE   Memory Mapped Register
  //                     "1000"   C_BASEADDR + 0x0
  //                     "0100"   C_BASEADDR + 0x4
  //                     "0010"   C_BASEADDR + 0x8
  //                     "0001"   C_BASEADDR + 0xC
  // 
  // ------------------------------------------------------

  assign
    slv_reg_write_sel = Bus2IP_WrCE[0:2],
    slv_reg_read_sel  = Bus2IP_RdCE[0:2],
    slv_write_ack     = Bus2IP_WrCE[0] || Bus2IP_WrCE[1] || Bus2IP_WrCE[2],
    slv_read_ack      = Bus2IP_RdCE[0] || Bus2IP_RdCE[1] || Bus2IP_RdCE[2];

  // implement slave model register(s)
  always @( posedge Bus2IP_Clk )
    begin//: SLAVE_REG_WRITE_PROC
	 
	 //slv_reg2[0] <= Interrupt;
	 slv_reg2[C_SLV_DWIDTH-1] <= interruptreset;
	 
      if ( Bus2IP_Reset == 1 )
        begin
          //slv_reg0 <= 0;
          //slv_reg1 <= 0;
          slv_reg2 <= 0;
        end
      else
        case ( slv_reg_write_sel )
          3'b100 :
            for ( byte_index = 0; byte_index <= (C_SLV_DWIDTH/8)-1; byte_index = byte_index+1 )
              if ( Bus2IP_BE[byte_index] == 1 )
                for ( bit_index = byte_index*8; bit_index <= byte_index*8+7; bit_index = bit_index+1 )
		begin

		end
                  //slv_reg0[bit_index] <= Bus2IP_Data[bit_index];
          3'b010 :
            for ( byte_index = 0; byte_index <= (C_SLV_DWIDTH/8)-1; byte_index = byte_index+1 )
              if ( Bus2IP_BE[byte_index] == 1 )
                for ( bit_index = byte_index*8; bit_index <= byte_index*8+7; bit_index = bit_index+1 )
		begin

		end
                  //slv_reg1[bit_index] <= Bus2IP_Data[bit_index];
          3'b001 :
            for ( byte_index = (C_SLV_DWIDTH/8)-2; byte_index <= (C_SLV_DWIDTH/8)-1; byte_index = byte_index+1 )
              if ( Bus2IP_BE[byte_index] == 1 )
                for ( bit_index = byte_index*8; bit_index <= byte_index*8+7; bit_index = bit_index+1 )
                  slv_reg2[bit_index] <= Bus2IP_Data[bit_index];
          default : ;
        endcase

    end  // SLAVE_REG_WRITE_PROC

  // implement slave model register read mux
  always @( slv_reg_read_sel or slv_reg0 or slv_reg1 or slv_reg2 )
    begin: SLAVE_REG_READ_PROC

      case ( slv_reg_read_sel )
        3'b100 : slv_ip2bus_data <= slv_reg0;
        3'b010 : slv_ip2bus_data <= slv_reg1;
        3'b001 : slv_ip2bus_data <= slv_reg2;
        default : slv_ip2bus_data <= 0;
      endcase

    end // SLAVE_REG_READ_PROC

  // ------------------------------------------------------------
  // Example code to drive IP to Bus signals
  // ------------------------------------------------------------

  assign IP2Bus_Data    = slv_ip2bus_data;
  assign IP2Bus_WrAck   = slv_write_ack;
  assign IP2Bus_RdAck   = slv_read_ack;
  assign IP2Bus_Error   = 0;

endmodule

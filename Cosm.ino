// COSM change these settings to match your own setup
#define APIKEY  "Your Key Here" 

char cosmURL[] PROGMEM = "api.cosm.com";


void sendToCosm(byte cosmData, char* cosmFeed)
{
	uint8_t cosmip[4] = {216,52,233,121};
	
		ether.copyIp(ether.hisip, cosmip);
    
		// generate the header with payload - note that the stash size is used,
		// and that a "stash descriptor" is passed in as argument using "$H"
		Stash::prepare(PSTR("PUT http://$F/v2/feeds/$F.csv HTTP/1.0" "\r\n"
							"Host: $F" "\r\n"
							"X-PachubeApiKey: $F" "\r\n"
							"Content-Length: $D" "\r\n"
							"\r\n"
							"$H"),
				cosmURL, cosmFeed, cosmURL, PSTR(APIKEY), stash.size(), cosmData);

		// send the packet - this also releases all stash buffers once done
		ether.tcpSend();
		
		if (stash.freeCount() <= 3) 
		{
			Stash::initMap(56);
		}
		
#if DEBUG
		Serial.print("Feed: ");
		Serial.print(int(cosmFeed));
		Serial.print(", Sh: ");
		Serial.println(stash.freeCount());
#endif
}
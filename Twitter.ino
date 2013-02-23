//Twitter Setup
// supertweet.net username:password in base64
// http://tuxgraphics.org/~guido/javascript/base64-javascript.html
#define KEY "your key here, see above"

char twitterURL[] PROGMEM = "api.supertweet.net";

void sendToTwitter (byte tweetText) 
{
	uint8_t twitterip[4] = {66, 180, 175, 246}; 
	ether.copyIp(ether.hisip, twitterip);
	
	// generate the header with payload - note that the stash size is used,
	// and that a "stash descriptor" is passed in as argument using "$H"
	Stash::prepare(PSTR("POST /1/statuses/update.xml HTTP/1.1" "\r\n"
          						"Host: $F" "\r\n"
          						"Authorization: Basic $F" "\r\n"
          						"User-Agent: Arduino EtherCard lib" "\r\n"                        
          						"Content-Length: $D" "\r\n"
          						"Content-Type: application/x-www-form-urlencoded" "\r\n"
          						"\r\n"
          						"$H"),
					        twitterURL, PSTR(KEY), stash.size(), tweetText);
	
	// send the packet - this also releases all stash buffers once done
	ether.tcpSend();
	
	if (stash.freeCount() <= 3) 
		{
			Stash::initMap(56);
		}
}
var ip;

//Called when application is started.
function OnStart()
{
  //Create a layout with objects vertically centered.
  lay = app.CreateLayout( "linear", "VCenter,FillXY" );   
  app.AddLayout( lay );
    
  //Create an information text label and add it to layout.
  txt = app.CreateText( "Connecting ..." );
  txt.SetTextSize( 32 );
  lay.AddChild( txt );    
    
    
  //Create UDP network object.
  net = app.CreateNetClient( "UDP" );
  Scan();
  if (ip == "undefined") {
  txt.SetTextColor('red');
  txt.SetTextSize(16);
  txt.SetText("Nepripojený");
  } else {
  txt.SetTextColor('green');
  txt.SetTextSize(16);
  txt.SetText("Pripojený k " + ip);
  var url = "http://"+ip;
  }
  
      
  //Create a button to send request.
  btnInc = app.CreateButton( "Ďalší", 0.8, 0.2, "FillX,Gray"); 
  btnInc.SetMargins( 0, 0.05, 0, 0 ); 
  btnInc.SetOnTouch( btn_QueueInc_OnTouch ); 
  btnInc.SetTextSize(36);
  lay.AddChild( btnInc ); 
  
  //Create a button to send request.
  btnDec = app.CreateButton( "Predchádzajúci", 0.8, 0.2, "FillX,Gray"); 
  btnDec.SetMargins( 0, 0.05, 0, 0 ); 
  btnDec.SetOnTouch( btn_QueueDec_OnTouch ); 
  btnDec.SetTextSize(36);
  lay.AddChild( btnDec ); 
  
  /*
 //Create a button to find display IP
  btnScan = app.CreateButton( "Scan...", 0.8, 0.2, "FillX,Gray"); 
  btnScan.SetMargins( 0, 0.05, 0, 0 ); 
  btnScan.SetOnTouch( Scan ); 
  btnScan.SetTextSize(36);
  lay.AddChild( btnScan ); 
*/


    function btn_QueueInc_OnTouch()
    {
            //Send request to remote server.
            var path = "/queue.html";
            var params = "counter=next";
            app.HttpRequest( "get", url, path, params);
            app.Vibrate("0,20");
    }

    function btn_QueueDec_OnTouch()
    {
            //Send request to remote server.
            var path = "/queue.html";
            var params = "counter=previous";
            app.HttpRequest( "get", url, path, params );
            app.Vibrate("0,20");
    }




    function Scan()
    {
        //Try to read a packet for 1 millisec.
    	var packet = net.ReceiveDatagram( "UTF-8", 19700, 5000 );
    	
    	if( packet ) 
    	{ 
    	    //Extract IP address
    	    ip = packet;
            //app.ShowPopup( ip );
    	}
    }

    //Handle the servers reply.
    function HandleReply( error, response )
    {
          console.log(error);
          app.ShowPopup(response);
    }
}
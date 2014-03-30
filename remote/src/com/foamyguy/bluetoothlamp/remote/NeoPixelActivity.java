package com.foamyguy.bluetoothlamp.remote;

import android.app.Activity;
import android.os.Bundle;
import android.os.Handler;
import android.util.Log;
import android.view.Menu;
import android.view.View;
import android.view.View.OnClickListener;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.SeekBar;
import android.widget.SeekBar.OnSeekBarChangeListener;
import android.widget.Spinner;
import android.widget.Toast;

public class NeoPixelActivity extends Activity {
	TBlue blu;
	Spinner devicesSpn;
	Button connectBtn;
	SeekBar redSeek;
	SeekBar blueSeek;
	SeekBar greenSeek;
	Button sendBtn;
	String curColor = "0 0 0";
	ArrayAdapter<String> spinnerArrayAdapter;

	@Override
	protected void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
		setContentView(R.layout.activity_neo_pixel);
		
		blu = new TBlue();
		devicesSpn = (Spinner) findViewById(R.id.devicesSpn);
		connectBtn = (Button) findViewById(R.id.connectBtn);
		redSeek = (SeekBar)findViewById(R.id.redSeek);
		greenSeek = (SeekBar)findViewById(R.id.greenSeek);
		blueSeek = (SeekBar)findViewById(R.id.blueSeek);
		//sendBtn = (Button)findViewById(R.id.sendBtn);
		spinnerArrayAdapter = new ArrayAdapter<String>(this, R.layout.simple_spinner_dropdown_item, blu.getDeviceList());

        devicesSpn.setAdapter(spinnerArrayAdapter);
        

        final Handler h = new Handler();
        final Runnable sendColorRun = new Runnable(){
        	public void run() {
        		Log.i("BlueToothFun", curColor);
        		if (blu.isConnected()){
        			blu.write("rgb -1 " + curColor + " 1");
        		}
        		
        	}
        };
        //h.post(sendColorRun);
        
        connectBtn.setOnClickListener(new OnClickListener() {
        	public void onClick(View v){
        		
        		if(blu.isConnected() == false){
	        		int selectedPos = devicesSpn.getSelectedItemPosition();
	        		String mac = spinnerArrayAdapter.getItem(selectedPos).split("\n")[1];
	        		boolean connected = blu.connect(mac);
	        		if(connected){
	        			Toast.makeText(v.getContext(), "Connected", Toast.LENGTH_SHORT).show();
	        			connectBtn.setText("Disconnect");
	        		}
        		}else{
        			try {
						blu.close();
					} catch (Exception e) {
						// TODO Auto-generated catch block
						e.printStackTrace();
					}
        			connectBtn.setText("Connect");
        		}
        		
        	}
        });
        
        redSeek.setOnSeekBarChangeListener(new OnSeekBarChangeListener(){

			@Override
			public void onProgressChanged(SeekBar v, int seek, boolean arg2) {
				curColor = seek + " " + greenSeek.getProgress() + " " + blueSeek.getProgress();
				h.postDelayed(sendColorRun, 800);
				
			}

			@Override
			public void onStartTrackingTouch(SeekBar arg0) {
				// TODO Auto-generated method stub
				
			}

			@Override
			public void onStopTrackingTouch(SeekBar arg0) {
				// TODO Auto-generated method stub
				
			}
        	
        });
        
        
        blueSeek.setOnSeekBarChangeListener(new OnSeekBarChangeListener(){

			@Override
			public void onProgressChanged(SeekBar v, int seek, boolean arg2) {
				curColor = redSeek.getProgress() + " " + greenSeek.getProgress() + " " + seek;
				h.postDelayed(sendColorRun, 800);
				
			}

			@Override
			public void onStartTrackingTouch(SeekBar arg0) {
				// TODO Auto-generated method stub
				
			}

			@Override
			public void onStopTrackingTouch(SeekBar arg0) {
				// TODO Auto-generated method stub
				
			}
        	
        });
        
        
        greenSeek.setOnSeekBarChangeListener(new OnSeekBarChangeListener(){

			@Override
			public void onProgressChanged(SeekBar v, int seek, boolean arg2) {
				curColor = redSeek.getProgress() + " " + seek + " " + blueSeek.getProgress();
				h.postDelayed(sendColorRun, 800);
				
			}

			@Override
			public void onStartTrackingTouch(SeekBar arg0) {
				// TODO Auto-generated method stub
				
			}

			@Override
			public void onStopTrackingTouch(SeekBar arg0) {
				// TODO Auto-generated method stub
				
			}
        	
        });
        

        
        
        /*sendBtn.setOnClickListener(new OnClickListener(){
        	public void onClick(View v){
        		blu.write("rgb -1 " + redSeek.getProgress() + " " + greenSeek.getProgress() + " " + blueSeek.getProgress() + " 1");
        	}
        });*/
	}

	

}

/*
 * Read mpdLibtest Histo mode output file
 *
 * Last update: Nov/2017
 * Author: E. Cisbani
 */

#include <unistd.h>
#include <Riostream.h>
#include <TSystem.h>
#include <TCanvas.h>
#include <TVector3.h>
#include <TStyle.h>
#include <TGraph.h>
#include <TTree.h>
#include <TH1F.h>
#include <TH2F.h>
#include <TStyle.h>
#include <TMath.h>
#include <TProfile.h>
#include <TString.h>
#include <TText.h>
#include <TCut.h>
#include <TLine.h>
#include <map>
void setStyle() {

  gStyle->SetCanvasColor(kWhite);     // background is no longer mouse-dropping white
  gStyle->SetCanvasDefW(500);
  gStyle->SetCanvasDefH(500);
  gStyle->SetPalette(1,0);            // blue to red false color palette. Use 9 for b/w
  gStyle->SetCanvasBorderMode(0);     // turn off canvas borders
  gStyle->SetPadBorderMode(0);
  gStyle->SetPaintTextFormat("5.2f");  // What precision to put numbers if plotted with "TEXT"

  gStyle->SetTextSize(0.08);
  gStyle->SetLabelSize(0.1,"xyz"); // size of axis value font
  gStyle->SetTitleSize(0.1,"xyz"); // size of axis title font
  gStyle->SetTitleFont(62,"xyz"); // font option
  gStyle->SetLabelFont(62,"xyz");
  gStyle->SetTitleOffset(0.8,"y"); 
  gStyle->SetTitleOffset(1.0,"x");

  gStyle->SetPadTopMargin(0.1);
  gStyle->SetPadRightMargin(0.1);
  gStyle->SetPadBottomMargin(0.16);
  gStyle->SetPadLeftMargin(0.1);

  gStyle->SetPadGridX(0);
  gStyle->SetPadGridY(0);
  gStyle->SetPadTickX(1);
  gStyle->SetPadTickY(1);
}

/*
 *
 * module if positive, select a single module
 *
 * fmean = 0 no mean values in data file
 * fmean = 1 mean values in data file  
 * fmean = 2 rms values data file (>7/nov 2015)
 *
 * iwin = [nA] current window around mean value
 *
 */
int readHisto(TString infile) {

  Int_t i;
   
  // clean input file and read relevant information

  /*
    gSystem->Exec(Form("awk '$1 ~ /^[#.0-9]+$/ { print $0 }' %s >temp.txt",infile.Data()));
    TString dummy = gSystem->GetFromPipe("awk 'BEGIN {flg=0;} $2==\"Time=\"&&flg==0 { print $3; flg=1; }' temp.txt");
    Int_t time_off = dummy.Atoi();
    printf("Start absolute time = %d sec\n",time_off);
    dummy =  gSystem->GetFromPipe("awk 'BEGIN {flg=0;} $1!=\"#\"&&flg==0 { print NF; flg=1; }' temp.txt");
    Int_t nch = (dummy.Atoi()-2)/3;
    printf("Number of channel(s) in input file = %d\n",nch);
    TString stime = gSystem->GetFromPipe("awk 'BEGIN {flg=0;} $2==\"Time_Start=\"&&flg==0 { $1=$2=\"\"; print $0; flg=1; }' temp.txt");
    stime = stime.Strip(TString::kBoth);
    printf("Start Time = %s\n",stime.Data());
  */
  
  TTree *thm = new TTree("thm","MPD Histo Mode data");

  TString rform="mpd/I:clockphase/I:adc/I:gain/I:val[4096]/I";

  Int_t nrow = thm->ReadFile(infile.Data(),rform);

  printf("%d rows of data from input file %s\n", nrow, infile.Data());

  //  thm->Draw("val:Iteration$","(mpd==6)&&(adc==0)");

  thm->SetMarkerStyle(7);

  // count mpd
  thm->Draw("mpd","(adc==0)&&(clockphase==0)");
  Int_t nmpd = thm->GetSelectedRows();
  printf("Number of mpds in data = %d\n",nmpd);
  
  // count clockphase
  thm -> Draw("clockphase","(mpd==3)&&(adc==0)");
  Int_t nclockphase=thm->GetSelectedRows();
  printf("Number of clockphase in data = %d\n",nclockphase);
  
  //count how many ADCs
  thm ->Draw("adc","(mpd==3)&&(clockphase==0)");
  Int_t nadc=thm->GetSelectedRows();
  printf("Number of adc channels in data = %d\n",nadc);
  
  // count mpd x adc
  thm->Draw("mpd:clockphase:adc");
  Int_t nscan = thm->GetSelectedRows();
  printf("Number of total scanned channels  = %d\n",nscan);


  Int_t *impd = new Int_t[nscan];
  Int_t *iclockphase=new Int_t[nscan];
  Int_t *iadc = new Int_t[nscan];
  Int_t *igain= new Int_t[nscan];
  for (Int_t i=0;i<nscan;i++) {
    impd[i] = thm->GetV1()[i];
    iclockphase[i]=thm->GetV2()[i];
    iadc[i] = thm->GetV3()[i];
    printf("%d : %d  %d  %d %d\n",i,impd[i],iclockphase[i],iadc[i],igain[i]);
  }

  
  Int_t mpd_v;
  Int_t adc_v;
  Int_t clockphase_v;
  Int_t gain_v;
  Int_t val_v[4096];
  thm->SetBranchAddress("mpd",&mpd_v);
  thm->SetBranchAddress("clockphase",&clockphase_v);
  thm->SetBranchAddress("adc",&adc_v);
  thm->SetBranchAddress("gain",&gain_v);
  thm->SetBranchAddress("val",val_v);
//  for(Int_t i =0;i<thm->GetEntries();i++){
//  thm->GetEntry(i);
//  	printf(" %5d: %5d %5d %5d %5d\n",i,mpd_v,clockphase_v,adc_v,gain_v);
//  }

  std::map<Int_t,std::map<Int_t,TH2D *>> ClockphaseHisto;
  for(Int_t i = 0; i < thm->GetEntries();i++){
  	thm->GetEntry(i);
  	
  	if(ClockphaseHisto.find(mpd_v)==ClockphaseHisto.end()||ClockphaseHisto[mpd_v].find(adc_v)==ClockphaseHisto[mpd_v].end()){
  	printf("Working on mpd %d , adc %d\n",mpd_v,adc_v);
  	ClockphaseHisto[mpd_v][adc_v]=new TH2D(Form("MPD%d_APV%d",mpd_v,adc_v),Form("MPD%d",mpd_v,adc_v),65,0,64,4097,0,4096);
  	ClockphaseHisto[mpd_v][adc_v]->GetXaxis()->SetTitle("clock_phase");
  	ClockphaseHisto[mpd_v][adc_v]->GetYaxis()->SetTitle("ADC");
  	}
  	
  	double high_vol=0;
  	double high_index=0;
  	double low_vol=0;
  	double low_index=0;
  	for(Int_t val_iter=0;val_iter<4096;val_iter++){
  		if(val_iter>1300){
  		high_vol+=val_iter*val_v[val_iter];
  		high_index+=val_v[val_iter];
  		}else{
  		  low_vol+=val_iter*val_v[val_iter];
  		  low_index+=val_v[val_iter];
  		}
  		
  		
  	}
  	ClockphaseHisto[mpd_v][adc_v]->Fill(clockphase_v,int(high_vol/high_index));
  	ClockphaseHisto[mpd_v][adc_v]->Fill(clockphase_v,int(low_vol/low_index));
  	ClockphaseHisto[mpd_v][adc_v]->SetMarkerStyle(20);
  	int color=632;
  	if(adc_v<4){
  		color=kGreen+adc_v;
  	}else if(adc_v<8){
  		color=kCyan+adc_v-4;
  	}else if(adc_v<12){
  		color=kBlue+adc_v-8;
  	}else if(adc_v<16){
  		color=kMagenta+adc_v-12;
  	}else{
  		color=kRed+adc_v-16;
  	}
  	ClockphaseHisto[mpd_v][adc_v]->SetMarkerColor(color);
  	ClockphaseHisto[mpd_v][adc_v]->SetFillColor(color);

  } 
  
  // Draw the graph
  TCanvas **cc=new TCanvas*[ClockphaseHisto.size()];
  Int_t canvas_count_temp=0;
  for(auto iter=ClockphaseHisto.begin();iter!=ClockphaseHisto.end();iter++){
  	cc[canvas_count_temp]=new TCanvas(Form("cmpd%d",canvas_count_temp),Form("MPD %d",iter->first));
  	cc[canvas_count_temp]->cd();
  	for(auto iter_adc=iter->second.begin();iter_adc!=iter->second.end();iter_adc++){
  	iter_adc->second->Draw("same");
  	}
  	
  	cc[canvas_count_temp]->Update();
  	gPad->BuildLegend(0.78,0.695,0.980,0.935,"","f");
  	canvas_count_temp++;
  }
  
  return 0;

}

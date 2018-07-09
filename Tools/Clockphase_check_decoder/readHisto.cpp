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

  TString rform="mpd/I:adc/I:gain/I:val[4096]/I";

  Int_t nrow = thm->ReadFile(infile.Data(),rform);

  printf("%d rows of data from input file %s\n", nrow, infile.Data());

  //  thm->Draw("val:Iteration$","(mpd==6)&&(adc==0)");

  thm->SetMarkerStyle(7);

  // count mpd
  thm->Draw("mpd","adc==0");
  Int_t nmpd = thm->GetSelectedRows();

  printf("Number of mpds in data = %d\n",nmpd);
  
  // count mpd x adc
  thm->Draw("mpd:adc");

  Int_t nscan = thm->GetSelectedRows();

  printf("Number of total scanned channels  = %d\n",nscan);

  Int_t *impd = new Int_t[nscan];
  Int_t *iadc = new Int_t[nscan];

  for (Int_t i=0;i<nscan;i++) {
    impd[i] = thm->GetV1()[i];
    iadc[i] = thm->GetV2()[i];
    //  printf("%d : %d %d\n",i,impd[i],iadc[i]);
  }

  TCanvas **cc = new TCanvas*[nmpd];
  for (Int_t i=0;i<nmpd;i++) {
    printf("Prepare cancas %d for mpd %d\n",i,impd[i]);
    cc[i] = new TCanvas(Form("cmpd%d",i),Form("MPD %d",impd[i]));
    cc[i]->Divide(4,4);
    cc[i]->Update();
  }

  // now plot

  TGraph **gr = new TGraph*[nscan];
 
  Int_t j=0; // mpd idx
  for (Int_t i=0;i<nscan;i++) {
    // printf("%d %d : %d %d\n",i,j,impd[i],iadc[i]);
    cc[j]->cd(1+iadc[i])->SetLogy(1);
    thm->Draw("val:Iteration$",Form("(mpd==%d)&&(adc==%d)",impd[i],iadc[i]));
    gr[i] = new TGraph(thm->GetSelectedRows(), thm->GetV2(), thm->GetV1());
    gr[i]->SetMinimum(1);
    gr[i]->SetMarkerStyle(6);
    gr[i]->Draw("pawl");
    cc[j]->Update();
    j++;
    j = j % nmpd;
  }


  return 0;

}

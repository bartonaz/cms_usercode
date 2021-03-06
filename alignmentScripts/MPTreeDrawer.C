#include "TROOT.h"
#include "TString.h"
#include "TFile.h"
#include "TStyle.h"
#include "TGraph.h"
#include "TGraphErrors.h"
#include "TTree.h"
#include "TList.h"
#include "TKey.h"
#include "TH1F.h"
#include "TGaxis.h"
#include "TLegend.h"
#include "TLegendEntry.h"
#include "TCanvas.h"
#include "TLatex.h"

#include <exception>
#include <cstdio>

#include "/afs/cern.ch/user/n/nbartosi/cms/TkAl/MillePede/CMSSW_5_3_3_patch2/src/DataFormats/DetId/interface/DetId.h"
#include "/afs/cern.ch/user/n/nbartosi/cms/TkAl/MillePede/CMSSW_5_3_3_patch2/src/DataFormats/SiPixelDetId/interface/PXBDetId.h"
#include "/afs/cern.ch/user/n/nbartosi/cms/TkAl/MillePede/CMSSW_5_3_3_patch2/src/DataFormats/SiPixelDetId/src/PXBDetId.cc"
#include "/afs/cern.ch/user/n/nbartosi/cms/TkAl/MillePede/CMSSW_5_3_3_patch2/src/DataFormats/SiPixelDetId/interface/PXFDetId.h"
#include "/afs/cern.ch/user/n/nbartosi/cms/TkAl/MillePede/CMSSW_5_3_3_patch2/src/DataFormats/SiPixelDetId/src/PXFDetId.cc"
#include "/afs/cern.ch/user/n/nbartosi/cms/TkAl/MillePede/CMSSW_5_3_3_patch2/src/DataFormats/SiStripDetId/interface/TIBDetId.h"
#include "/afs/cern.ch/user/n/nbartosi/cms/TkAl/MillePede/CMSSW_5_3_3_patch2/src/DataFormats/SiStripDetId/interface/TOBDetId.h"


#include "lumisInIOV.C"   // Luminosity of each run range of each IOV

int histoIdx ( UInt_t detId );
void setGraphStyle ( TGraph *graph, int id, int file );
void drawEmptyHisto ( float Xmin=0, float Xmax=20, TString xTitle="", float Ymin=0, float Ymax=1, TString yTitle="", TString name="empty" );
void setLegendStyle ( TLegend *leg,int ncols, int nDetParts );
void setTDRStyle();
void tdrGrid(bool gridOn);
void fixOverlay();


TStyle *tdrStyle;
int markerColors[2]= {1,2};
int markerStyles[6][2]= {{20,24},{21,25},{22,26},{23,32}};
int lineStyles[3]= {1,7,3};
int lineColors[8]= {13,kPink-9,kAzure+7,kSpring-6,kOrange+1,kPink-1,kAzure+10,kSpring+9};
int fillStyles[2]= {1001,3008};
TString inputFileName = "/afs/cern.ch/cms/CAF/CMSALCA/ALCA_TRACKERALIGN/MP/MPproduction/mp1326/jobData/jobm/treeFile_merge.root";
TString outputPath = "/afs/cern.ch/cms/CAF/CMSALCA/ALCA_TRACKERALIGN/MP/MPproduction/mp1326/LA_evol";
std::vector<TString> DetName;
TString calibrationType = "LorentzAngle";
int nRings[4] = {8,2,6,6};
int nLayers[4] = {3,1,4,6};
int DetIndex = -1;	// 0-BPIX 1-FPIX 2-TIB 3-TOB
int VLayer = 1;	// [BPIX] Layers: 1-3 Rings: 1-8 [FPIX] Side: 0-1 [TIB] Layers: 1-4 Modules: 1-6 [TOB] Layers: 1-6 Modules: 1-4
int LayerRing = 0; // Meaning of VLayer: 0-Layer; 1-Ring (Module);
int StripReadMode = 0;  // Strip readout mode: 0-peak 1-deco
int nPointsToSkip=1;	// Number of points that should be skipped from the end
//float lumiScale=0.826;
float lumiScale=1.0;
bool drawLegend=true;
float minY=0.f;
float maxY=0.f;
bool autoScaleY = false;
bool drawInput = true;      // Whether to draw lines for input LA
FILE *logFile;
FILE *runsFile;

bool canvasSquare=false;

//float iovWidth = 0.33;	  // Width of one IOV in fb-1 for labels of X axis
float fixedIOVwidth[4] = {0.35, 0.35, 1.27, 1.27};      // Width of one IOV in fb-1 for each DetId
bool iovWidthIsFixed = false;                           // Whether to compute real width of which IOV based on its luminosity
float minY_det[4] = {0.35,-0.095,0.06,0.065};           // BPIX, FPIX, TIB, TOB
float maxY_det[4] = {0.46,-0.075,0.082,0.11};           // BPIX, FPIX, TIB, TOB



int MPTreeDrawer (TString detName, int structId, int layerRing, int stripReadoutMode=0)
{
    setTDRStyle();

    DetName.push_back("BPIX");
    DetName.push_back("FPIX");
    DetName.push_back("TIB");
    DetName.push_back("TOB");

    for(unsigned int i=0; i<DetName.size(); i++) {
        if(DetName.at(i)!=detName) continue;
        DetIndex=i;
        break;
    }
    if(DetIndex<0) {
        printf("Wrong detector name. Possible names are:");
        for(unsigned int i=0; i<DetName.size(); i++) printf("%s ",DetName.at(i).Data());
            printf("\nStopping...");
        return 1;
    }
    VLayer = structId;
    LayerRing = layerRing;
    StripReadMode = stripReadoutMode;

    if(nLayers[DetIndex]==1) { LayerRing=0; VLayer=0; }


    logFile = fopen ( "log.txt","w" );

    runsFile = fopen ( "runs.txt","w" );

    TFile *file = new TFile ( inputFileName );

    TString moduleType="";
    TString readoutMode="";
    if ( DetIndex<2 ) {
        moduleType="Pixel";

    } else {
        moduleType="Strip";
        readoutMode = (StripReadMode==0?"_peak":"_deconvolution");

    }
    TString TreeBaseDir("Si"+moduleType+calibrationType+"Calibration"+readoutMode+"_result_");

    std::vector<int> iovs;
    std::vector<int> iovs_;
    double totLumi=0.0;
    int nIOVs;

// Finding the IOV borders from the directory names in each ROOT file
    TList *list = file->GetListOfKeys();
    int nKeys = list->GetSize();
    for ( int keyNum=0; keyNum<nKeys; keyNum++ ) {
        TKey* key = ( TKey* ) list->At ( keyNum );
        TString str ( key->GetName() );
        if ( !str.BeginsWith ( TreeBaseDir ) ) {
            continue;
        }
        str.ReplaceAll ( TreeBaseDir,"" );
        int run = str.Atoi();
// Putting run number to the list of IOVs of the file
        iovs.push_back ( run );
        iovs_.push_back ( run );
    }
    nIOVs=iovs.size();
    if(nIOVs<1) {
        printf("No trees found with name: %s...\nExiting...\n",TreeBaseDir.Data());
        return -1;
    }
// Changing the run number if it is 1
    if ( iovs_[0]==1 && nIOVs>2 ) {
        iovs_[0]=2*iovs[1]-iovs[2];
    }
    iovs_[0]=190000;	// Setting the first run of the first IOV to a custom value
    iovs_.push_back(209091);	// Setting the last run of the last IOV to a custom value


    // Direct values from the tree
    UInt_t detId;
    Float_t value;
    // Struct of additional values from the tree
    struct treeStr{
        Float_t delta;
        Float_t error;
        UInt_t paramIndex;
    };
    Float_t error=0.f;
    treeStr treeStruct;
    std::vector<TGraphErrors*> graphLA;
    std::vector<TGraphErrors*> graphLAinput;
    int nDetParts=-1;
    bool isOldFormat = false;

    // Looping over entries in each iov tree
    for ( int iov=0; iov<nIOVs+1; iov++ ) {
        char treeName[300];
        sprintf ( treeName,"%s%d",TreeBaseDir.Data(),iovs[iov] );
    // Reading tree for input tree
        if(iov==nIOVs) {
            TString TreeBaseDirIn = TreeBaseDir;
            TreeBaseDirIn.ReplaceAll("_result_","_input");
            sprintf ( treeName,"%s",TreeBaseDirIn.Data());
        }
        TTree *tree = 0;
        tree = ( TTree* ) ( TDirectoryFile* ) file->Get ( treeName );
        Long64_t nEntries = tree->GetEntries();

        int runNr = (iov<nIOVs)?iovs.at(iov):555;
        if(tree) printf ( "Got Tree %d\twith name: %s for IOV: %d - %lld entries\n",iov,treeName,runNr,nEntries );

        fprintf(runsFile,"%d\n",iovs_.at(iov));
        if(iov>=nIOVs-nPointsToSkip && iov<nIOVs) continue;

        tree->SetBranchAddress ( "detId",&detId );
        tree->SetBranchAddress ( "value",&value );
        if(tree->GetBranch("error")) isOldFormat = true;
        if(isOldFormat) tree->SetBranchAddress ( "error",&error ); else
        tree->SetBranchAddress ( "treeStruct",&treeStruct );


        double iovWidth=-1.f;
        if(iovWidthIsFixed || runNr==555) iovWidth = fixedIOVwidth[DetIndex]; 
        else {
    // Getting more precise value of luminosity (calculation started from the first IOV run)
            iovWidth = lumisInIOV(iovs_.at(0),iovs_.at(iov+1)) - lumisInIOV(iovs_.at(0),iovs_.at(iov));
            if(iovWidth<0.0) {
    // Getting less precise value of luminosity (calculation started from this IOV run)
                iovWidth = lumisInIOV(iovs_.at(iov),iovs_.at(iov+1));
                printf("Less precise estimation of luminosity for IOV: %d (%d-%d)\n",iov+1,iovs_.at(iov),iovs_.at(iov+1));
            }
        }
        if(iovWidth<0 && iov<nIOVs) {
            printf("   ERROR!!! Luminosity for IOV %d with runs: %d - %d not found. Skipping.\n",iov,iovs_.at(iov),iovs_.at(iov+1));
            continue;
        }
        iovWidth*=lumiScale;                           // Correcting luminosity to the real (lumiCalc provides slightly larger value)
        if(!iovWidthIsFixed) iovWidth/=1000.0;         // Converting from /pb to /fb
        totLumi+=iovWidth;                             // Updating total luminosity of all IOVs


        for ( Long64_t entry=0; entry<nEntries; entry++ ) {
        //             printf("  Entry %lld\n",entry);
            tree->GetEntry ( entry );
            if(!isOldFormat) error = treeStruct.error;
            int histoId=histoIdx ( detId );
            // fprintf ( logFile,"  entry: %lld\thistoId: %d\tvalue: %.3f\terror: %.3f\n",entry,histoId,value,error );
            if ( histoId<0 ) {
                continue;
            }
            while(histoId>=(int)graphLA.size() && iov<nIOVs) {
                TGraphErrors* graph = new TGraphErrors ( nIOVs-nPointsToSkip );
                graphLA.push_back(graph);
            }
            while(histoId>=(int)graphLAinput.size() && iov>=nIOVs) {
        //printf("0. histoId: %d size: %d\n",histoId,(int)graphLAinput.size());
                TGraphErrors* graph = new TGraphErrors ( 1 );
                graphLAinput.push_back(graph);
            }
            if ( DetIndex==0 || DetIndex==2 || DetIndex==3 ) {
                if(iov<nIOVs) {
                    graphLA.at(histoId)->SetPoint ( iov, totLumi - 0.5*iovWidth, value*3.81 );	// BPIX, TIB, TOB
                    graphLA.at(histoId)->SetPointError ( iov,0.f,error*3.81 );
                } else {    // For line of input LA value
                //printf("1. histoId: %d size: %d\n",histoId,(int)graphLAinput.size());
                    Double_t centerY;
                    Double_t centerX;
                    graphLA.at(histoId)->GetPoint(graphLA.at(histoId)->GetN()/2,centerX,centerY);
                    graphLAinput.at(histoId)->SetPoint ( 0, centerX, value*3.81 );	// BPIX, TIB, TOB
                    graphLAinput.at(histoId)->SetPointError ( 0,centerX*2.5,error*3.81 );
                }
            } else if ( DetIndex==1 ) {
                if(iov<nIOVs) {
                    graphLA.at(histoId)->SetPoint ( iov, totLumi + 0.5*iovWidth, value* ( -1.3 ) );	// FPIX
                    graphLA.at(histoId)->SetPointError ( iov,0.f,error*1.3 );
                } else {    // For line of input LA value
                    Double_t centerY;
                    Double_t centerX;
                    graphLA.at(histoId)->GetPoint(graphLA.at(histoId)->GetN()/2,centerX,centerY);
                    graphLAinput.at(histoId)->SetPoint ( 0, centerX + 0.5*iovWidth, value* ( -1.3 ) );	// FPIX
                    graphLAinput.at(histoId)->SetPointError ( 0,centerX*2.5,error*1.3 );
                }
            }
        }	  // End of loop over entries
    }	// End of loop over IOVs
    nDetParts=graphLA.size();
    printf("Found %d different substructures\n",nDetParts);
    // if(LayerRing==0) nRings[DetIndex]=nDetParts;        // Updating the number of rings to draw
    if(nDetParts<1) {
        fclose(logFile);
        fclose(runsFile);
        return 1;
    }

    float minY_ = minY_det[DetIndex];
    float maxY_ = maxY_det[DetIndex];

    if(minY!=0.f || maxY!=0.f) {
        minY_ = minY;
        maxY_ = maxY;
    }

    if(autoScaleY) {
        minY_=999.9;
        maxY_=-999.9;
    }
    fprintf ( logFile,"File: %s\n",inputFileName.Data() );
    for ( int i=0; i<nDetParts; i++ ) {
    // fprintf ( logFile,"ID: %d Values: ",i );
        for ( int j=0; j<graphLA.at(i)->GetN(); j++ ) {
    // Updating min and max values of LA for axis of the plot
            Double_t val;
            Double_t null;
            graphLA.at(i)->GetPoint ( j,null,val );
            fprintf(logFile,"detPart: %d\tiov %d\tRun: %d\tValue: %.3f\n",i+1,j+1,iovs.at(j),val);
            if ( val<minY_ && autoScaleY && val!=0.0) {
                minY_=val;
            }
            if ( val>maxY_ && autoScaleY && val!=0.0) {
                maxY_=val;
            }
    // fprintf ( logFile," %.3f",val );
        }
    // fprintf ( logFile,"\n" );
    }	// End of loop over Detector parts

    if(autoScaleY) {
        minY_= ( minY_>0 ) ?minY_*0.98:minY_*1.02;
        maxY_= ( maxY_>0 ) ?maxY_*1.05:maxY_*0.95;
    }

    //Drawing canvas
    //    TCanvas *c1 = new TCanvas ( "c1","Canvas1",1000,600 );
    TCanvas *c1 = new TCanvas ( "c1","Canvas1");
    // Drawing empty histogram
    TString Y_title;
    if(calibrationType=="LorentzAngle") Y_title = "tan(#theta_{LA}^{shift}) "; else
    if(calibrationType=="Backplane") Y_title = "#DeltaW_{BP}^{shift} "; else
    Y_title = "??? ";
    drawEmptyHisto ( 0.0,totLumi,"2012 Integrated Luminosity [fb^{-1}]",minY_,maxY_,Y_title,"empty1" );
    // Drawing each graph for input values
    if(drawInput) {
        for ( int i=0; i<nDetParts; i++ ) {
            setGraphStyle ( graphLAinput.at(i),i,1 );
            graphLAinput.at(i)->SetMarkerStyle(0);
            graphLAinput.at(i)->Draw( "Lsame" );
        }
    }
    // Drawing each graph for output values
    for ( int i=0; i<nDetParts; i++ ) {
        setGraphStyle ( graphLA.at(i),i,0 );
        graphLA.at(i)->Draw ( "Psame" );
    }

    TString structName = ( LayerRing==0 ) ?"L":"R";
    TString structNameFull = ( LayerRing==0 ) ?"Layer":"Ring";

    //Drawing legend pane
    TLegend *leg;
    leg = new TLegend ( 0.7,0.77,0.98,0.95,NULL,"brNDC" );
        
    int nCols = (nDetParts>3)?2:1;

    for(int i=0; i<=nDetParts/2; i++) {
        int i2=0;
        TString legName_("");
        if(LayerRing==1) {
            i2 = i+nDetParts/2;
            legName_+="Layer";
        }
        if(LayerRing==0) {
            i2 = nDetParts-1-i;
            legName_+="Ring";
        }
        if(nDetParts<4) {
            i2 = i*2+1;
            i = i2-1;
        }
        char legName[100];
        if(i<nDetParts/2) {
            sprintf(legName,"%s %d",legName_.Data(),i+1);
            leg->AddEntry( graphLA.at(i),legName,"p" );
            sprintf(legName,"%s %d",legName_.Data(),i2+1);
            leg->AddEntry( graphLA.at(i2),legName,"p" );
            if(i==nDetParts/2-1 && nDetParts%2==0) break;
        } else {
            sprintf(legName,"%s %d",legName_.Data(),i+1);
            leg->AddEntry( graphLA.at(i),legName,"p" );
        }
    }
    setLegendStyle ( leg,nCols,nDetParts);


    if ( drawLegend ) {
        leg->Draw();
    }

    //    // Drawing CMS Preliminary label
    //    TLatex *TextCMS = new TLatex(0.2,0.89,"CMS Preliminary 2012");
    //    TextCMS->SetTextColor(1);
    //    TextCMS->SetNDC();
    //    TextCMS->SetTextFont(62);
    //    TextCMS->Draw();

    char Data_[150];
    sprintf(Data_,"%s %s %d",DetName.at(DetIndex).Data(),structNameFull.Data(),VLayer);
    TLatex *TextData = new TLatex(0.2,0.89,Data_);
    TextData->SetTextColor(1);
    TextData->SetNDC();
    TextData->SetTextFont(62);
    TextData->Draw();

    char savePath[150];

    gROOT->ProcessLine(".mkdir -p "+outputPath);
    sprintf ( savePath,"%s/%s_%s_%s%d%s",outputPath.Data(),calibrationType.Data(),DetName.at(DetIndex).Data(),structName.Data(),VLayer, readoutMode.Data() );
    c1->Print( TString ( savePath ) +=".eps" );
    //    c1->SaveAs ( TString ( savePath ) +=".pdf" );
    //    c1->SaveAs ( TString ( savePath ) +=".C" );
    //    c1->SaveAs ( TString ( savePath ) +=".root" );

    fclose ( logFile );

    fclose ( runsFile );

    return 0;
}

int histoIdx ( UInt_t detId )
{
    DetId det_ ( detId );
//    fprintf(logFile,"  detId: %d\tdet: %d\tsubDetId: %d\t", detId, det_.det(), det_.subdetId());
    if ( det_.det() !=DetId::Tracker ) {
//        fprintf(logFile,"\n");
        return -1;
    }

    int id=-1;
    int layer=-1;
    int ring=-1;

    switch(DetIndex) {
        case 0: if(det_.subdetId() == PixelSubdetector::PixelBarrel) {
            PXBDetId det(detId);
            layer = det.layer();
            ring = det.module();
            if(LayerRing==0 && layer==VLayer) id = ring-1;
            if(LayerRing==1 && ring==VLayer) id = layer-1;
        } break;
        case 1: if(det_.subdetId() == PixelSubdetector::PixelEndcap) {
            PXFDetId det(detId);
            id = det.side()-1;
        } break;
        case 2: if(det_.subdetId() == StripSubdetector::TIB) {
            TIBDetId det(detId);
            layer = det.layer();
            ring = det.module();
            int side = det.side();
//            fprintf(logFile,"layer: %d\tring: %d\tside: %d",layer,ring,side);
            int modulesInStruct = 6/nRings[DetIndex];

            if(side==1) ring = 3-ring; else
            if(side==2) ring = 3+ring-1;
            ring = ring/modulesInStruct;

            if(LayerRing==0 && layer==VLayer) id = ring;
            if(LayerRing==1 && ring+1==VLayer) id = layer-1;
        } break;
        case 3: if(det_.subdetId() == StripSubdetector::TOB) {
            TOBDetId det(detId);
            layer = det.layer();
            ring = det.module();
            int side = det.side();
//            fprintf(logFile,"layer: %d\tring: %d\tside: %d\t",layer,ring,side);
            int modulesInStruct = 12/nRings[DetIndex];

            if(side==1) ring = 6-ring; else
            if(side==2) ring = 6+ring-1;
            ring = ring/modulesInStruct;

            if(LayerRing==0 && layer==VLayer) id = ring;
            if(LayerRing==1 && ring+1==VLayer) id = layer-1;
        } break;
    }
//    fprintf(logFile,"\n");

    return id;
}

void setGraphStyle ( TGraph *graph, int id, int file )
{
    int structId = -1;
    if(LayerRing==0) structId = ( id<nRings[DetIndex]/2 )?id:nRings[DetIndex]-1-id; else structId = id/2;

    int Zpart = ( id<nRings[DetIndex]/2 )?0:1;
    if(LayerRing==1) Zpart = id%2;
    if(LayerRing==0 && nRings[DetIndex]%2!=0) {
        Zpart=0;
        structId = id;
    }

    graph->SetMarkerStyle ( markerStyles[structId][Zpart] );
//    graph->SetMarkerSize ( 1 );
    graph->SetMarkerColor ( lineColors[structId] );
//    graph->SetFillStyle ( fillStyles[Zpart] );
    graph->SetLineColor ( lineColors[structId] );
    graph->SetLineWidth ( 2 );
    graph->SetLineStyle ( lineStyles[0] );
    if(file>0) graph->SetLineStyle ( lineStyles[Zpart] );
//    graph->SetFillColor ( lineColors[structId] );
//
//    graph->SetLineWidth ( file+1 );
}

void drawEmptyHisto ( float Xmin, float Xmax, TString xTitle, float Ymin, float Ymax, TString yTitle, TString name )
{
    printf ( "Setting empty histo: X: %.2f-%.2f\tY: %.2f-%.2f\n",Xmin,Xmax,Ymin,Ymax );
    printf("Total luminosity to be plotted: %.3f\n",Xmax);
    printf("Luminosity scale used: %.2f\n",lumiScale);

    Xmax*=1.02;	  // Increasing max lumi to have free space
    gStyle->SetOptStat ( 0 );
    //    TGaxis::SetMaxDigits ( 4 );

    TH1F *histo = new TH1F ( name, xTitle, 1, Xmin, Xmax );
    TAxis *xAx = histo->GetXaxis();
    TAxis *yAx = histo->GetYaxis();
    xAx->SetTitle ( xTitle );
    //    xAx->SetTitleOffset ( 1.1 );
    //    xAx->SetNdivisions ( 510 );
    yAx->SetTitle ( yTitle );
    //    yAx->SetTitleOffset ( 1.15 );
    //    yAx->SetNdivisions ( 510 );
    histo->SetMinimum ( Ymin );
    histo->SetMaximum ( Ymax );
    histo->Draw ( "AXIS" );
}

void setLegendStyle ( TLegend *leg,int ncols, int nDetParts )
{
    leg->SetTextFont ( 42 );
//    leg->SetTextSize ( 0.04 );
    leg->SetFillStyle ( 1001 );
    leg->SetBorderSize ( 1 );
//    leg->SetLineColor ( 1 );
    leg->SetFillColor ( 0 );
    leg->SetNColumns ( ncols );
    if(ncols==1) {
        leg->SetX1(leg->GetX1() + 0.5*(leg->GetX2() - leg->GetX1() ));
    }
    leg->SetY1(leg->GetY2() - nDetParts/ncols*(0.05));
}

// Public CMS style (Detector Performance)

// tdrGrid: Turns the grid lines on (true) or off (false)

void tdrGrid(bool gridOn) {
    tdrStyle->SetPadGridX(gridOn);
    tdrStyle->SetPadGridY(gridOn);
}

// fixOverlay: Redraws the axis

void fixOverlay() {
    gPad->RedrawAxis();
}

void setTDRStyle() {
    tdrStyle = new TStyle("tdrStyle","Style for P-TDR");

// For the canvas:
    tdrStyle->SetCanvasBorderMode(0);
    tdrStyle->SetCanvasColor(kWhite);
    if(canvasSquare) {
        tdrStyle->SetCanvasDefH(600); //Height of canvas
        tdrStyle->SetCanvasDefW(600); //Width of canvas
    } else {
        tdrStyle->SetCanvasDefH(800); //Height of canvas
        tdrStyle->SetCanvasDefW(1200); //Width of canvas
    }
    tdrStyle->SetCanvasDefX(0);   //POsition on screen
    tdrStyle->SetCanvasDefY(0);

    // For the Pad:
    tdrStyle->SetPadBorderMode(0);
    // tdrStyle->SetPadBorderSize(Width_t size = 1);
    tdrStyle->SetPadColor(kWhite);
    tdrStyle->SetPadGridX(false);
    tdrStyle->SetPadGridY(false);
    tdrStyle->SetGridColor(0);
    tdrStyle->SetGridStyle(3);
    tdrStyle->SetGridWidth(1);

    // For the frame:
    tdrStyle->SetFrameBorderMode(0);
    tdrStyle->SetFrameBorderSize(1);
    tdrStyle->SetFrameFillColor(0);
    tdrStyle->SetFrameFillStyle(0);
    tdrStyle->SetFrameLineColor(1);
    tdrStyle->SetFrameLineStyle(1);
    tdrStyle->SetFrameLineWidth(1);

    // For the histo:
    // tdrStyle->SetHistFillColor(1);
    // tdrStyle->SetHistFillStyle(0);
    tdrStyle->SetHistLineColor(1);
    tdrStyle->SetHistLineStyle(0);
    tdrStyle->SetHistLineWidth(1);
    // tdrStyle->SetLegoInnerR(Float_t rad = 0.5);
    // tdrStyle->SetNumberContours(Int_t number = 20);

    tdrStyle->SetEndErrorSize(2);
    //  tdrStyle->SetErrorMarker(20);
    tdrStyle->SetErrorX(0.);

    tdrStyle->SetMarkerStyle(20);
    if(!canvasSquare) tdrStyle->SetMarkerSize(2);

    //For the fit/function:
    tdrStyle->SetOptFit(1);
    tdrStyle->SetFitFormat("5.4g");
    tdrStyle->SetFuncColor(2);
    tdrStyle->SetFuncStyle(1);
    tdrStyle->SetFuncWidth(1);

    //For the date:
    tdrStyle->SetOptDate(0);
    // tdrStyle->SetDateX(Float_t x = 0.01);
    // tdrStyle->SetDateY(Float_t y = 0.01);

    // For the statistics box:
    tdrStyle->SetOptFile(0);
    tdrStyle->SetOptStat(0); // To display the mean and RMS:   SetOptStat("mr");
    tdrStyle->SetStatColor(kWhite);
    tdrStyle->SetStatFont(42);
    tdrStyle->SetStatFontSize(0.025);
    tdrStyle->SetStatTextColor(1);
    tdrStyle->SetStatFormat("6.4g");
    tdrStyle->SetStatBorderSize(1);
    tdrStyle->SetStatH(0.1);
    tdrStyle->SetStatW(0.15);
    // tdrStyle->SetStatStyle(Style_t style = 1001);
    // tdrStyle->SetStatX(Float_t x = 0);
    // tdrStyle->SetStatY(Float_t y = 0);

    // Margins:
    tdrStyle->SetPadTopMargin(0.05);
    tdrStyle->SetPadBottomMargin(0.13);
    tdrStyle->SetPadLeftMargin(0.16);
    tdrStyle->SetPadRightMargin(0.02);

    // For the Global title:

    tdrStyle->SetOptTitle(0);
    tdrStyle->SetTitleFont(42);
    tdrStyle->SetTitleColor(1);
    tdrStyle->SetTitleTextColor(1);
    tdrStyle->SetTitleFillColor(10);
    tdrStyle->SetTitleFontSize(0.05);
    // tdrStyle->SetTitleH(0); // Set the height of the title box
    // tdrStyle->SetTitleW(0); // Set the width of the title box
    // tdrStyle->SetTitleX(0); // Set the position of the title box
    // tdrStyle->SetTitleY(0.985); // Set the position of the title box
    // tdrStyle->SetTitleStyle(Style_t style = 1001);
    // tdrStyle->SetTitleBorderSize(2);

    // For the axis titles:

    tdrStyle->SetTitleColor(1, "XYZ");
    tdrStyle->SetTitleFont(42, "XYZ");
    tdrStyle->SetTitleSize(0.06, "XYZ");
    // tdrStyle->SetTitleXSize(Float_t size = 0.02); // Another way to set the size?
    // tdrStyle->SetTitleYSize(Float_t size = 0.02);
    tdrStyle->SetTitleXOffset(0.9);
    tdrStyle->SetTitleYOffset(1.25);
    // tdrStyle->SetTitleOffset(1.1, "Y"); // Another way to set the Offset

    // For the axis labels:

    tdrStyle->SetLabelColor(1, "XYZ");
    tdrStyle->SetLabelFont(42, "XYZ");
    tdrStyle->SetLabelOffset(0.007, "XYZ");
    tdrStyle->SetLabelOffset(0.0075, "Y");
    tdrStyle->SetLabelSize(0.05, "XYZ");

    // For the axis:

    tdrStyle->SetAxisColor(1, "XYZ");
    tdrStyle->SetStripDecimals(kTRUE);
    tdrStyle->SetTickLength(0.03, "XYZ");
    tdrStyle->SetNdivisions(510, "XYZ");
    tdrStyle->SetPadTickX(0);  // To get tick marks on the opposite side of the frame
    tdrStyle->SetPadTickY(0);

    // Change for log plots:
    tdrStyle->SetOptLogx(0);
    tdrStyle->SetOptLogy(0);
    tdrStyle->SetOptLogz(0);

    // Postscript options:
    tdrStyle->SetPaperSize(20.,20.);
    // tdrStyle->SetLineScalePS(Float_t scale = 3);
    // tdrStyle->SetLineStyleString(Int_t i, const char* text);
    // tdrStyle->SetHeaderPS(const char* header);
    // tdrStyle->SetTitlePS(const char* pstitle);

    // tdrStyle->SetBarOffset(Float_t baroff = 0.5);
    // tdrStyle->SetBarWidth(Float_t barwidth = 0.5);
    // tdrStyle->SetPaintTextFormat(const char* format = "g");
    // tdrStyle->SetPalette(Int_t ncolors = 0, Int_t* colors = 0);
    // tdrStyle->SetTimeOffset(Double_t toffset);
    // tdrStyle->SetHistMinimumZero(kTRUE);

    tdrStyle->cd();

}


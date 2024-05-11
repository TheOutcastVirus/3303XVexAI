#include "vex.h"
#include "OverUnder.h"
#include <robot-config.h>
#include <algorithm>
#include "field.h"

using namespace OverUnder;


Field::Field(vex::color Alliance_Color)
{
    Side = Alliance_Color;
    Snap_Path = &Path2Snap2;
    for(int i = 0; i < (Snap_Path->PathPoints.size() - 1); i++)
    {
        Line* newLine = new Line();
        newLine->LinePoints[0] = Snap_Path->PathPoints[i];
        newLine->LinePoints[1] = Snap_Path->PathPoints[i+1];
        Snap_Path_Lines.push_back(*newLine);
       
    }
    Field_Barriers.push_back(&Center);
    Field_Barriers.push_back(&Blue_Goal);
    Field_Barriers.push_back(&Red_Goal);

    Goal_Zone.push_back(&Red_BL_Corner);
    Goal_Zone.push_back(&Red_FL_Corner);
    Goal_Zone.push_back(&Red_FR_Corner);
    Goal_Zone.push_back(&Red_BR_Corner);

}

pair <Point*, double> Field::Find_Closest_Point_In_Line(Point* point,Line LineSeg)
{
    pair <Point*, double> Data ;
    double
    Ax = LineSeg.LinePoints[0]->Xcord,
    Ay = LineSeg.LinePoints[0]->Ycord,
    Bx = LineSeg.LinePoints[1]->Xcord,
    By = LineSeg.LinePoints[1]->Ycord,
    Pointx = point->Xcord,
    Pointy = point->Ycord;

    double Px = Bx - Ax;
    double Py = By - Ay;
    double temp = (Px*Px) + (Py*Py);
    double U = ((Pointx - Ax) * Px + (Pointy - Ay) * Py) / (temp);
    if(U > 1)
    {
        U = 1;
    }
    else if(U < 0)
    {
        U = 0;
    }
    double X = Ax + U * Px;
    double Y = Ay + U * Py;
    double Dx = X - Pointx;
    double Dy = Y - Pointy;
    double Dist = sqrt((Dx * Dx) + (Dy * Dy));

    Point* Point_On_Line = new Point(X,Y);
    Data.first = Point_On_Line;
    Data.second = Dist;

    return Data;

}

int orientation(Point* p, Point* q, Point* r) 
{ 
    double val = (q->Ycord - p->Ycord) * (r->Xcord - q->Xcord) - (q->Xcord - p->Xcord) * (r->Ycord - q->Ycord); 
    if (val == 0) 
        return 0;  // collinear 
  
    return (val > 0)? 1: 2; // clock or counterclock wise 
}

bool Check_Intersects(Point* CurrentPos, Point* PointOnLine, Line* BarrierLine)
{
    Point* Line_PointA = BarrierLine->LinePoints[0];
    Point* Line_PointB = BarrierLine->LinePoints[1];
    int o1 = orientation(CurrentPos, PointOnLine, Line_PointA); 
    int o2 = orientation(CurrentPos, PointOnLine, Line_PointB); 
    int o3 = orientation(Line_PointA, Line_PointB, CurrentPos); 
    int o4 = orientation(Line_PointA, Line_PointB, PointOnLine); 
    // General case 
    if (o1 != o2 && o3 != o4) 
        return true;

    return false; // Doesn't fall in any of the above cases 
}

bool Field::Check_Barrier_Intersects(Point* CPos, Point* POL)
{
    bool Intersect;
    for(int i = 0; i < Field_Barriers.size(); i++)
    {
        for(int j = 0; j < Field_Barriers[i]->BarrierLines.size(); j++)
            
            Intersect = Check_Intersects(CPos,POL,Field_Barriers[i]->BarrierLines[j]);
            if(Intersect)
                return Intersect;
    }
    return false;
}


bool Field::In_Goal_Zone(double Ball_x, double Ball_y)
{
    int num_vertices = Goal_Zone.size();
    double x = abs(Ball_x), y = Ball_y;
    bool inside = false;

    Point* P1 = Goal_Zone[0];
    Point* P2;
    // Loop through each edge in the polygon
    for (int i = 1; i <= num_vertices; i++) 
    {
       P2 = Goal_Zone[i % num_vertices];
        if (y > min(P1->Ycord, P2->Ycord)) 
        {
            if (y <= max(P1->Ycord, P2->Ycord)) 
            {
                if (x <= max(P1->Xcord, P2->Xcord)) 
                {
                    double x_intersection = (y - P1->Ycord) * (P2->Xcord - P1->Xcord) / (P2->Ycord - P1->Ycord) + P1->Xcord;
                    if (P1->Xcord == P2->Xcord || x <= x_intersection) 
                    {
                        inside = !inside;
                    }
                }
            }
        }
        P1 = P2;
    }
    return inside;
}


bool pairCompare(const std::pair<Point*, double>& firstElem, const std::pair<Point*, double>& secondElem) 
{
  return firstElem.second < secondElem.second;
}

pair<Point*, int> Field::Find_Point_on_Path(Point* freePoint)
{
    vector<pair<Point*, double>> Point_Dist;
    vector<pair<Point*, double>> Line_Pos; // This serves as a copy of the above vector
    pair<Point*,int> Point_LinePos;

    for(int i = 0; i < Snap_Path_Lines.size() - 1; i++)
    {
        Point_Dist.push_back(Find_Closest_Point_In_Line(freePoint,Snap_Path_Lines[i]));
        Line_Pos.push_back(Find_Closest_Point_In_Line(freePoint,Snap_Path_Lines[i]));
    }
    std::sort(Point_Dist.begin(),Point_Dist.end(), pairCompare);
    for(int j = 0; j < Point_Dist.size(); j++)
    {
        if(!Check_Barrier_Intersects(freePoint,Point_Dist[j].first))
        {
            Point_LinePos.first = Point_Dist[j].first;
            for(int k = 0; k < Point_Dist.size(); k++)
            {
                if(Point_Dist[j].first == Line_Pos[k].first)
                    Point_LinePos.second = k + 1;
            }

            return Point_LinePos;
        }
    }
    return Point_LinePos;

}

Path Field::Create_Path_to_Target(Point* Target)
{
    Path DrivePath;
    int StartingLine;
    int EndingLine;
    int LineBetween; 
   
    Point* CurrentPos = new Point(GPS.xPosition(vex::distanceUnits::cm),GPS.yPosition(vex::distanceUnits::cm));
    pair<Point*, int> Start = Find_Point_on_Path(CurrentPos);
    DrivePath.PathPoints.push_back(Start.first);
    StartingLine = Start.second;
    pair<Point*,int> End = Find_Point_on_Path(Target);
    EndingLine = End.second;

    if(StartingLine > EndingLine) // SL = 4 EL= 1 // SL = 10 EL = 1 
    {
        LineBetween = StartingLine - EndingLine;  // = -3 // = 11
        if(abs(LineBetween) > Snap_Path_Lines.size()/2) 
        {
            LineBetween = Snap_Path_Lines.size() - StartingLine + EndingLine; // = +3
        }
    }
    else // SL = 1 EL= 4 // SL = 1 EL = 10
    {
        LineBetween = EndingLine - StartingLine; // = 3 // = -11
        if(abs(LineBetween) > Snap_Path_Lines.size()/2) 
        {
            LineBetween = EndingLine - Snap_Path_Lines.size() - StartingLine;   // = -3
        }
    }

    if(LineBetween < 0)   // 0 12 11 
    {
        for(int i = StartingLine - 1; i == EndingLine - 1 ; i--)
        {
            DrivePath.PathPoints.push_back(Snap_Path->PathPoints[i]);
            if(i == 0 )
            {
                i = Snap_Path->PathPoints.size();
            }
        }
    }
    else   // 10:1          1 2 3  
    {
         for(int i = StartingLine; i == EndingLine  ; i++)  // 10 11 12 13 
        {
            DrivePath.PathPoints.push_back(Snap_Path->PathPoints[i]);
            if(i == Snap_Path->PathPoints.size())
            {
                i = 0 ;
            }
        }
    }
    DrivePath.PathPoints.push_back(End.first);
    DrivePath.PathPoints.push_back(Target);
    return DrivePath ;

}

#pragma once

#include <map>
#include <vector>
#include "sc2api/sc2_api.h"
#include "DistanceMap.h"

#include "BaseLocation.h"

class CCBot;

enum SCVSTATE {
	MovingToMineral,
	GatheringMineral,
	ReturningMineral,
	ExtraMoveToMineral, //used for the path trick
	ExtraMoveToPos,
	NoTrick //used for the path trick
};

enum Algorithm {
	BuildIn,
	EvenSplit,
	Queue,
	EvenSplitPath,
	QueuePath,
	Single,
	Search
};

struct SCV {

	const sc2::Unit* unit;
	SCVSTATE SCVstate;
	const sc2::Unit* mineral;

	//using these to measure Mining SpeedUp
	int StartedMoving; //frame in which the scv started moving
	int StartedMining; //frame in which the scv started mining

	int nextReturnTime; //next time (or frame#) that unit will return a cargo of 5 minerals (use for future resource-use planning)
	//int bestRoundTrip; //#frames from return to return for the same mineral

	sc2::Point3D Intermediate; //point in front of crystal to double-click sprint to, in order to skip slow-acceleration from normal pathing
	const sc2::Unit* interMineral; //other Crystal behind/next-to target Crystal in order to avoid slow-down-deceleration from normal pathing

	//std::vector< int > timeTilNextReturn; //matches with worker points, indicates next mineral return
};

struct Mineral {
	Mineral(const sc2::Unit* min) : mineralUnit(min), SCVcount(0), Facing(-1) {

		MinToCC = -1;
		CCToMin = -1;
		//mineralTrick = nullptr; Facing = -1; //posTrick don't need initialized
	}

	//CONST attributes
	const sc2::Unit* mineralUnit;
	//sc2::Point3D pos; //get this from sc2::Unit*->pos
	const sc2::Unit* mineralTrick;//mineral which allows for mineral trick to speed up SCV
	sc2::Point3D posTrick;//Position to allow path finding trick
	int Facing; //0 = top, 1 = right, 2 = bottom, 3 = left

	//DERIVED attributes
	int MinToCC; //return-to-TownHall trip, in frames
	int CCToMin; //return-to-Crystal trip, in frames
	int miningTime = 45;
	int roundTrip; //(MinToCC + CCToMin) + miningTime //will need updated for speedTrick bestTime
	
	//int workerCapacity; //get floor of ((MinToCC + CCToMin)/miningTime ). Close a bit >2. Far a bit >3?

	//DYNAMIC ATTRIBUTES
	int SCVcount; //this should just be Queue.size()
	std::vector< sc2::Tag > Queue; //queue with index for worker units tags
	
	//std::vector< SCV* > scvs; //to SCV structs
	//std::vector< sc2::Unit* > workers; //to actual unit structs

};


class BaseManager
{
public:

	CCBot & m_bot;   
	bool activate;

	BaseManager(CCBot & bot);
    
	const BaseLocation*  baseData;
	Algorithm CurrentAlg;
	int current_frame;

	//std::vector< const sc2::Unit* > workers;
	std::vector< SCV > workers;
	std::vector< Mineral > minerals;

	void onStart(const BaseLocation*   startData );
	void onFrame();

	void OnUnitCreated(const sc2::Unit* unit);

	const sc2::Unit * getBuilder();

	void onFrameBuiltIn();
	void onFrameSplit();
	void onFrameQueue();
	void onFramePath();
	void onFrameQueuePath();
	void onFrameSingle();

	void addWorkerBuiltIn(const sc2::Unit* unit);
	void addWorkerSplit(const sc2::Unit* unit);
	void addWorkersQueue(const sc2::Unit* unit);
	void addWorkersPath(const sc2::Unit* unit, const sc2::Unit* townHall);
	void addWorkersQueuePath(const sc2::Unit* unit);
	void addWorkersSingle(const sc2::Unit* unit);

	void firstFrameWorkerSpread();
	std::vector<int> allocateMineralWorkers(const sc2::Units & workers, const sc2::Units & mins, const sc2::Unit* townHall);//, bool keepCurrent);

	const sc2::Unit * getClosestDepot(const sc2::Unit * worker) const;
	void ScheduleWorker(int index);
	void RemoveWorkerSchedule(int index);

	int GetOrientation(const sc2::Unit* newMineral);
	const sc2::Unit* GetMineralTrick(const sc2::Unit* newMineral, int facing);
	sc2::Point3D TrickPos(const sc2::Unit* newMineral, int facing);

};

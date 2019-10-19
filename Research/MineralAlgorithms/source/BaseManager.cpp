#include "BaseManager.h"
#include "Util.h"
#include "CCBot.h"
#include <sstream>
#include <iostream>

const int NearBaseLocationTileDistance = 20;



bool isLeft(sc2::Point2D a, sc2::Point2D b, sc2::Point2D c) {
	return ((b.x - a.x)*(c.y - a.y) - (b.y - a.y)*(c.x - a.x)) >= 0;
}

#define max(a,b) (((a) > (b)) ? (a) : (b))

BaseManager::BaseManager(CCBot & bot)
	: m_bot(bot)
{
	//m_bot = bot;

	workers.clear();
	minerals.clear();
	activate = false;

}



int BaseManager::GetOrientation(const sc2::Unit* newMineral) {

	baseData->getDepotPosition();
	//sc2::Point2D TopLeft(baseData->getDepotPosition().x - 5, 

	int distance = 5;
	sc2::Point2D TopLeft(baseData->getDepotPosition().x - distance, baseData->getDepotPosition().y + distance);
	sc2::Point2D TopRight(baseData->getDepotPosition().x + distance, baseData->getDepotPosition().y + distance);
	sc2::Point2D BottomLeft(baseData->getDepotPosition().x - distance, baseData->getDepotPosition().y - distance);
	sc2::Point2D BottomRight(baseData->getDepotPosition().x + distance, baseData->getDepotPosition().y - distance);

	sc2::Point2D mineral2dPos = newMineral->pos;

	if (isLeft(TopLeft, BottomRight, mineral2dPos) && isLeft(BottomLeft, TopRight, mineral2dPos)) {
		return 0; //facing top
	}
	if (isLeft(TopLeft, BottomRight, mineral2dPos) && !isLeft(BottomLeft, TopRight, mineral2dPos)) {
		return 1; //facing right
	}
	if (!isLeft(TopLeft, BottomRight, mineral2dPos) && !isLeft(BottomLeft, TopRight, mineral2dPos)) {
		return 2; //facing bottom
	}
	if (!isLeft(TopLeft, BottomRight, mineral2dPos) && isLeft(BottomLeft, TopRight, mineral2dPos)) {
		return 3; //facing left
	}


	return -1;

}

//i think Mineral Facing / Mineral Trick is only useful for Brood War and not SC2 ... //possibly useful on Close minerals to maintain mineral-walk incorporealness vs double-right-click speedup/non-slowdown
const sc2::Unit* BaseManager::GetMineralTrick(const sc2::Unit* newMineral, int facing)
{

	for (auto mineral2 : baseData->getMinerals() ) {
		float getDist = Util::Dist(newMineral->pos, mineral2->pos);
		if (getDist != 0.0 && getDist < 2) { // if this mineral is close

			if (facing == 0 && mineral2->pos.y > newMineral->pos.y) {
				return mineral2;
			}
			if (facing == 2 && mineral2->pos.y < newMineral->pos.y) {
				return mineral2;
			}
			if (facing == 1 && mineral2->pos.x > newMineral->pos.x) {
				return mineral2;
			}
			if (facing == 3 && mineral2->pos.x < newMineral->pos.x) {
				return mineral2;
			}
		}
	}
	return nullptr;
}

//find closest TownHall to dump resource at
const sc2::Unit * BaseManager::getClosestDepot(const sc2::Unit * worker) const
{
	const sc2::Unit * closestDepot = nullptr;
	double closestDistance = INT_MAX; //std::numeric_limits<double>::max();

	//go thru all your own TownHalls
	for (auto & unit : m_bot.UnitInfo().getUnits(Players::Self))
	{
		if (!unit) { continue; }

		if (Util::IsTownHall(unit) && Util::IsCompleted(unit)) //if townhall and not still being built
		{
			double distance = Util::DistSq(unit->pos, worker->pos);
			if (!closestDepot || distance < closestDistance)
			{
				closestDepot = unit;
				closestDistance = distance;
			}
		}
	}

	return closestDepot;
}

sc2::Point3D BaseManager::TrickPos(const sc2::Unit* newMineral, int facing)
{
	const sc2::Unit* closestDepot = getClosestDepot(newMineral);
	if (closestDepot == nullptr) return sc2::Point3D(0, 0, 0); //in case there are no TownHalls left (or they're flying :)
	sc2::Point3D depotPosition = closestDepot->pos;
	
	//must give a position right in front of crystal, closest to base //base location??

	float xOffset, yOffset;

	if (depotPosition.x < 60.0f) xOffset = 0.1f; else xOffset = -0.1f;
	if (depotPosition.y < 60.0f) yOffset = 0.1f; else yOffset = -0.1f;

	return sc2::Point3D(newMineral->pos.x + xOffset, newMineral->pos.y + yOffset, newMineral->pos.z);

	//first check if there is only 1 or less mineral field close to this one
	/*
	int minfieldsClose = 0;
	const sc2::Unit* minFieldClose = nullptr;
	for (auto mineral2 : baseData->getMinerals()) {
		float getDist = Util::Dist(newMineral->pos, mineral2->pos);
		if (getDist != 0.0 && getDist < 2) { // if this mineral is close
			minFieldClose = mineral2;
			minfieldsClose++;
		}
	}

	if (minfieldsClose > 1) { return 	sc2::Point3D(0, 0, 0); }

	if (facing == 0 ) {
		if (minFieldClose == nullptr)					{ return sc2::Point3D(newMineral->pos.x - 2 , newMineral->pos.y + 2, newMineral->pos.z); }
		if (minFieldClose->pos.x > newMineral->pos.x)	{ return sc2::Point3D(newMineral->pos.x - 1,  newMineral->pos.y + 2, newMineral->pos.z); }
		else {
			return sc2::Point3D(newMineral->pos.x + 1, newMineral->pos.y + 2, newMineral->pos.z);
		}
	}
	return 	sc2::Point3D(0, 0, 0);
	*/
}


void BaseManager::ScheduleWorker(int worker_index)
{
	//is this the standing_gather rate in Frames?
	const int Mining_Frames = 30; //mining takes about 30 frames? //more like 46?

	//is this just the standing_gather rate? or the roundtrip time ????
	const float Mining_Time_f = 9.0f; //about the travel time of 8.0f?

	//this needs better precision
	const float SpeedUnitsPerFrame = 2.8125 / 30.0; //how many speed units traveled each frame //30 frames per second? ya?

	int bestMineralField = -1; //initialize the best Crystal to go to
	float FastestEstimate = 99999.0; //int Fastest = 99999; //fastest_estimated???

	//iterate through all Crystals in MineralField
	for (int j = 0; j < minerals.size(); j++) {
		if (minerals[j].SCVcount > 1) continue; //ignore if Crystal has 2+ paired workers

		float TotalWork = 0; //was 'int'

		float DistToCC = Util::Dist(baseData->getDepotPosition(), minerals[j].mineralUnit->pos);

		int FramesDistToCC = (int)(DistToCC / SpeedUnitsPerFrame);

		float distanceThisSCVMineral = Util::Dist(workers[worker_index].unit->pos, minerals[j].mineralUnit->pos);

		int framesNeededThisToMineral = (int)(distanceThisSCVMineral / SpeedUnitsPerFrame);

		std::cerr << "Dist: " << distanceThisSCVMineral << "\n";

		for (int i = 0; i < minerals[j].Queue.size(); i++) {
			SCV* referencedSCV = nullptr;

			//why doesn't mineral have a direct pointer to its Workers? just a tag?
			for (int k = 0; k < workers.size(); k++) {
				if (workers[k].unit->tag == minerals[j].Queue[i]) {
					referencedSCV = &workers[k];
				}
			}

			if (referencedSCV == nullptr) {
				std::cerr << "TAG NOT FOUND!!!";
				continue;
			}

			float distanceSCVMineral = Util::Dist(referencedSCV->unit->pos, minerals[j].mineralUnit->pos);
			int framesNeeded = (int)(distanceSCVMineral / SpeedUnitsPerFrame);

			std::cerr << "Frame:" << current_frame << " - travel dist: " << distanceSCVMineral << "\n";

			float travel = max(0, distanceSCVMineral - TotalWork);
			float miningTime = Mining_Time_f; //drill takes about 30 frames

			//calculate remaining drilling time
			if (i == 0) {
				if (referencedSCV->SCVstate == GatheringMineral) {
					travel = 0.0f;
					int miningTimeframes = current_frame - referencedSCV->StartedMining;
					std::cerr << "Mining time: " << miningTime << "\n";
					if (miningTimeframes < 60 && miningTimeframes > 0) {
						miningTime = miningTime * (  (float)(60 - miningTimeframes) / 60.0f);
					}
					if (miningTimeframes >= 30) {
						std::cerr << "mining time is taking: " << miningTimeframes << "\n";
						//miningTime = 0.0f;
					}

					if (miningTime > Mining_Time_f) { //misrecorded state change //was 'int'
						miningTime = Mining_Time_f;
					}

				}
			}
			
			TotalWork += travel + miningTime;
			std::cerr << "Total work: " << TotalWork << " ";
		}
		float MyWork = max(0, distanceThisSCVMineral - TotalWork) + TotalWork + Mining_Time_f + DistToCC; //was 'int'

		if (FastestEstimate > MyWork) {
			FastestEstimate = MyWork;
			bestMineralField = j;
		}
	}

	std::cerr <<" FastestEstimate: "<< FastestEstimate << "  Index: " << worker_index << "\n";

	if (bestMineralField == -1) { std::cerr << "no suitable mineral field found"; }
	else {
		workers[worker_index].mineral = minerals[bestMineralField].mineralUnit;
		workers[worker_index].interMineral = minerals[bestMineralField].mineralTrick;
		workers[worker_index].Intermediate = minerals[bestMineralField].posTrick;

		minerals[bestMineralField].Queue.push_back(  workers[worker_index].unit->tag );
	}


}


void BaseManager::RemoveWorkerSchedule(int index)
{
	if (workers.size() <= index) { std::cerr << "index out of bounds" << workers.size() << " " << index << " "; }

	auto removeTag = workers[index].unit->tag;

	for (auto mineral : minerals) {
		for (int j = 0; j < mineral.Queue.size(); j++) {
			//manually remove workerTag from its target mineral's known Queue of workers
			if (mineral.Queue[j] == removeTag) {
				mineral.Queue.erase(mineral.Queue.begin() + j);
				break;
			}
		}
	}

	workers[index].mineral = nullptr; //wipe target mineral from de-assigned worker
}


std::ofstream fout;


void BaseManager::onStart(const BaseLocation*  startData)
{

	fout.open("MineralData.txt");
	std::ifstream fin;
	fin.open("AlgorithmChoice.txt");
	int choice = 0;
	fin >> choice;

	CurrentAlg = (Algorithm)choice;
	fin.close();

	baseData =  (startData);
	activate = true;
	current_frame = 0;

	auto depotPosition = baseData->getDepotPosition();
	auto closestMineralDist = 999.0f;
	UnitTag closestMineralTag;

	//auto list = m_bot.Observation()->GetUnits(sc2::Unit::Alliance::Self, sc2::IsUnit(sc2::UNIT_TYPEID::TERRAN_COMMANDCENTER)); //auto commandcenter = list.front(); //auto npos = commandcenter->pos;

	for (const sc2::Unit* mineral : baseData->getMinerals()) {

		Mineral newMineral = Mineral(mineral);//, depotPosition);
		newMineral.mineralUnit = mineral;
		newMineral.SCVcount = 0;
		newMineral.mineralTrick = nullptr;
		newMineral.Facing = GetOrientation(mineral);

		float distanceToTownHall = Util::Dist(mineral->pos, depotPosition);
		if (distanceToTownHall < closestMineralDist) {
			closestMineralTag = mineral->tag;
		}

		newMineral.posTrick = sc2::Point3D(0, 0, 0);
		if (newMineral.mineralTrick == nullptr) {
			newMineral.posTrick = TrickPos(mineral, newMineral.Facing);
		}

		minerals.push_back(newMineral);
	}

	float closestWorkerToClosestMineral = 999.0f;
	//UnitTag closestWorkerTag;

	for (auto unit : m_bot.Observation()->GetUnits(sc2::Unit::Self))
	{
		if (Util::IsWorker(unit))
		{   
			//updateWorker(unit);
			//workers.push_back(unit);
			OnUnitCreated(unit);

			//float distanceToClosestMineral = Util::Dist(unit->pos, baseData->getMinerals().closestMineralTag)
		}
	}

	//firstFrameWorkerSpread();
}

void BaseManager::onFrame()
{
	current_frame++;
	std::stringstream ss;
	ss << current_frame;
	std::string framesSTR = ss.str();

	//m_bot.Observation()->GetMinerals() is "Current Minerals", not including "Spent Minerals"

	if (current_frame % 250 == 0) {
		std::cerr << "Current Frames: " << current_frame << " Total Minerals: " << m_bot.Observation()->GetMinerals() << "\n";
	    fout << "Current Frames: " << current_frame << " Total Minerals: " << m_bot.Observation()->GetMinerals() << "\n";
		fout.flush();
	}

	//baseData->getPosition() , baseData->getDepotPosition() //draws at Base bottom-right X,Y -- i think
	m_bot.Map().drawTextScreen(sc2::Point2D(0.2f, 0.01f), "Frame:" + framesSTR); //draws in Top middle-left

	for (auto mineral : minerals ) {

		std::stringstream ss2;
		ss2 << mineral.Facing;
		//ss2 << mineral.Queue.size(); //This is how many Workers are targeting the mineral Crystal
		std::string facingSTR = ss2.str();
		m_bot.Map().drawText(mineral.mineralUnit->pos, facingSTR); //weird number on crystals
		

		int mineralsClose = 0;

		for (auto mineral2 : minerals) {
			float getDist = Util::Dist(mineral.mineralUnit->pos, mineral2.mineralUnit->pos);
			if (getDist != 0.0 && getDist < 2) {
				mineralsClose++;
			}
		}

		if (mineral.posTrick != sc2::Point3D(0,0,0) ) {
			m_bot.Map().drawLine(mineral.mineralUnit->pos, mineral.posTrick, sc2::Colors::Purple);
			m_bot.Map().drawSphere(mineral.posTrick, 1, sc2::Colors::Purple);
		}

	}

	switch (CurrentAlg) {
	case BuildIn:	onFrameBuiltIn();	break;
	case EvenSplit: onFrameSplit();		break;
	case Queue:		onFrameQueue();		break;
	case EvenSplitPath: onFramePath(); break;
	case QueuePath: onFrameQueuePath(); break;
	case Single: onFrameSingle();		break;
	}

}

void BaseManager::OnUnitCreated(const sc2::Unit * unit) //sc2::AppState; //m_bot.Control()->GetAppState(); //sc2::GameInfo; //m_bot.Observation().
{
	if (activate == false) { return; }

	if (Util::IsWorker(unit)) //std::cerr << " OnUnitCreated ";
	{
		bool alreadyThere = false;

		//this could be a map for constant time O(k) instead of linear time O(n)
		for (auto worker : workers) { //for (int i = 0; i < workers.size(); i++) {	
			if ( worker.unit->tag == unit->tag ) {
				alreadyThere = true; //worker just came out of gas refinery, or other transport (CC, medivac, nydus network, etc)
				break;
			}
		}

		if (alreadyThere == false ) { //std::cerr << " add unit " << CurrentAlg;
			//this should really grab the closest TownHall to the Worker, or ask Manager for which TownHall to go to
			auto th_list = m_bot.Observation()->GetUnits(sc2::Unit::Alliance::Self, sc2::IsUnit(sc2::UNIT_TYPEID::TERRAN_COMMANDCENTER));
			auto townHall = th_list.front(); //assume only 1 TownHall

			switch (CurrentAlg) {
			case BuildIn:	addWorkerBuiltIn(unit); break;
			case EvenSplit: addWorkerSplit(unit);	break;
			case Queue:		addWorkersQueue(unit);	break;
			case EvenSplitPath: addWorkersPath(unit, townHall); break;
			case QueuePath: addWorkersQueuePath(unit); break;
			case Single:	addWorkersSingle(unit); break;
			}
		}
	}
}

//only one worker going, so no interference
void BaseManager::onFrameSingle()
{
	for (auto unit : workers) {
		if (unit.unit->orders.size() > 0) {
			if (unit.unit->orders[0].ability_id == sc2::ABILITY_ID::HARVEST_GATHER && unit.unit->orders[0].target_unit_tag == unit.mineral->tag) { continue; }
			if (unit.unit->orders[0].ability_id == sc2::ABILITY_ID::HARVEST_RETURN) { continue; }
		}

		if (baseData->m_minerals.size() > 0) { Micro::SmartRightClick(unit.unit, unit.mineral, m_bot); }
	}
}

const sc2::Unit * BaseManager::getBuilder()
{ //std::cerr << "Total Workers " << workers.size();
	if (workers.size() > 0) {
		const sc2::Unit * buildUnit = workers[0].unit; //grab just the first worker in the Worker list

		if (CurrentAlg == Queue || CurrentAlg == QueuePath) { RemoveWorkerSchedule(0); }
		else { //remove SCV from total SCV counter
			for (int i = 0; i < minerals.size(); i++) { //std::cerr << i << " ";
				if (workers[0].mineral->tag == minerals[i].mineralUnit->tag) {
					minerals[i].SCVcount--;
				}
			}
		}
		workers.erase(workers.begin() + 0); //std::cerr << "SCV erased"; //workers.pop_back();
		return buildUnit;
	}
	return nullptr;
}

//this doesn't actually change anything
void BaseManager::onFrameBuiltIn()
{
	for (auto unit : workers) { 
		if (unit.unit->orders.size() > 0) {
			if (unit.unit->orders[0].ability_id == sc2::ABILITY_ID::HARVEST_GATHER) { continue; }
			if (unit.unit->orders[0].ability_id == sc2::ABILITY_ID::HARVEST_RETURN) { continue; }
		}
	}
}

//keeps workers on paired/assigned Mineral Crystal -- no changing
void BaseManager::onFrameSplit()
{
	for (auto unit : workers) {
		if (unit.unit->orders.size() > 0) {
			if (unit.unit->orders[0].ability_id == sc2::ABILITY_ID::HARVEST_GATHER && unit.unit->orders[0].target_unit_tag == unit.mineral->tag ) { continue; }
			if (unit.unit->orders[0].ability_id == sc2::ABILITY_ID::HARVEST_RETURN) { continue; }
		}

		if (baseData->m_minerals.size() > 0) { Micro::SmartRightClick(unit.unit, unit.mineral, m_bot); }
	}
}

void BaseManager::onFrameQueue()
{
	for (int i = 0; i < workers.size(); i++) {

		if (workers[i].SCVstate == MovingToMineral) {

			if (workers[i].unit->orders.size() > 0) { //skip workers that are just returning cargo/minerals to TownHall
				if (workers[i].unit->orders[0].ability_id == sc2::ABILITY_ID::HARVEST_RETURN) { continue; }
			}

			if (workers[i].mineral == nullptr) { //if worker not assigned to a Crystal
				ScheduleWorker(i); //doing Debug text here
				Micro::SmartRightClick(workers[i].unit, workers[i].mineral, m_bot); //continue;
			}

			float miningRange = (float)1.0; //if close enough to a mineral, start gathering from it
			if (Util::Dist(workers[i].unit->pos, workers[i].mineral->pos) < miningRange) {
				workers[i].SCVstate = GatheringMineral; //std::cerr << "changing state";
				workers[i].StartedMining = current_frame; //m_bot.Map().drawText(workers[i].scv->pos, workerText);

				auto min_pos = workers[i].mineral->pos;
				Micro::SmartRightClick(workers[i].unit, workers[i].mineral, m_bot);
				continue;
			}
		}

		if (workers[i].SCVstate == GatheringMineral) { //std::cerr << "gathering mineral";
			if (workers[i].mineral == nullptr) {
				workers[i].SCVstate = MovingToMineral; std::cerr << "!!!!!!! no mineral at Gathering Mineral";
			} //error would occur if mineral is exhausted while SCV en route to it for gathering

			if (workers[i].unit->orders.size() > 0) {
				if (workers[i].unit->orders[0].ability_id == sc2::ABILITY_ID::HARVEST_RETURN) { //worker just finished gathering and is now Returning: ChangeState
					workers[i].SCVstate = ReturningMineral;
					RemoveWorkerSchedule(i);

					continue;
				}
				//KEEP AT PAIRED/ASSIGNED CRYSTAL
				if ((workers[i].unit->orders[0].ability_id == sc2::ABILITY_ID::HARVEST_GATHER) //keep worker unit at its assigned/paired mineral field //if you are trying to harvest from a non-assigned mineral, got back to assigned target-mineral
					&& workers[i].unit->orders[0].target_unit_tag != workers[i].mineral->tag)
				{
					Micro::SmartRightClick(workers[i].unit, workers[i].mineral, m_bot);
					continue;
				}
			}
			else {
				Micro::SmartRightClick(workers[i].unit, workers[i].mineral, m_bot); //if set to Gather, but doesn't have an order yet, right-click on target mineral
				continue;
			}
		}

		if (workers[i].SCVstate == ReturningMineral) { //std::cerr << "return mineral"; //m_bot.Actions()->UnitCommand(workers[i].scv, sc2::ABILITY_ID::HARVEST_RETURN); //continue;

			if (workers[i].unit->orders.size() > 0) {
				if (workers[i].unit->orders[0].ability_id != sc2::ABILITY_ID::HARVEST_RETURN) {
					workers[i].SCVstate = MovingToMineral;
					//Micro::SmartRightClick(unit.scv, unit.mineral, m_bot);

					m_bot.Actions()->UnitCommand(workers[i].unit, sc2::ABILITY_ID::HARVEST_RETURN);
					continue;
				}
			}
			else {
				workers[i].SCVstate = MovingToMineral;
				continue;
				//let scheduler reschedule //Micro::SmartRightClick(workers[i].scv, workers[i].mineral, m_bot); //m_bot.Actions()->UnitCommand(unit.scv, sc2::ABILITY_ID::HARVEST_RETURN);
			}
		}
	}
}

//make sure the Intermediate point is only 
void BaseManager::onFramePath()
{
	for (int i = 0; i < workers.size(); i++) {

		if (workers[i].SCVstate == MovingToMineral  ) {
			//std::stringstream ss2; ss2 << Util::Dist(unit.scv->pos, unit.mineral->pos); std::string unitStr = ss2.str(); m_bot.Map().drawText(unit.scv->pos, unitStr);

			float gatherDist = (float)1.7;
			//if close enough to a mineral, start gathering from it
			if (Util::Dist(workers[i].unit->pos, workers[i].mineral->pos) < gatherDist) { //std::cerr << "close enough to gather";
				workers[i].SCVstate = GatheringMineral;
				Micro::SmartRightClick(workers[i].unit, workers[i].mineral, m_bot);
				continue;
			}

			//if we have an intermediate point, then move towards it
			if (workers[i].Intermediate != sc2::Point3D(0,0,0) ) {
				const sc2::Point2D  interpoint2d(workers[i].Intermediate);

				float maxDistInterPoint = (float)1.5; //if close enough to the inter point, start gathering from the real one
				if (Util::Dist(workers[i].unit->pos, workers[i].Intermediate) < maxDistInterPoint) {
					workers[i].SCVstate = GatheringMineral;
					Micro::SmartRightClick(workers[i].unit, workers[i].mineral, m_bot);
					continue;
				}

				if (workers[i].unit->orders.size() > 0) {
					if (workers[i].unit->orders[0].target_pos.x != interpoint2d.x && workers[i].unit->orders[0].target_pos.y != interpoint2d.y ) {
						Micro::SmartMove(workers[i].unit, workers[i].Intermediate, m_bot);
						continue;
					}
				}
				else {
					Micro::SmartMove(workers[i].unit, workers[i].Intermediate, m_bot); 
					continue;
				}
			} //unit.scv->orders[0].target_unit_tag == unit.mineral->tag)
		}

		if (workers[i].SCVstate == GatheringMineral) { //std::cerr << "gathering mineral";

			//
			if (workers[i].unit->orders.size() > 0) {

				if (workers[i].unit->orders[0].ability_id == sc2::ABILITY_ID::HARVEST_RETURN) {
					workers[i].SCVstate = ReturningMineral;
					continue;
				}
				//keep worker unit at its mineral field
				if ((workers[i].unit->orders[0].ability_id == sc2::ABILITY_ID::HARVEST_GATHER
					)
					&&
					workers[i].unit->orders[0].target_unit_tag != workers[i].mineral->tag
					) {
					Micro::SmartRightClick(workers[i].unit, workers[i].mineral, m_bot);
					continue;
				}
			}
			else {
				Micro::SmartRightClick(workers[i].unit, workers[i].mineral, m_bot);
				continue;
			}
		}

		if (workers[i].SCVstate == ReturningMineral) {
			//std::cerr << "return mineral";

			if (workers[i].unit->orders.size() > 0) {
				if (workers[i].unit->orders[0].ability_id != sc2::ABILITY_ID::HARVEST_RETURN) {
					workers[i].SCVstate = MovingToMineral;
					Micro::SmartRightClick(workers[i].unit, workers[i].mineral, m_bot);
					//m_bot.Actions()->UnitCommand(workers[i].unit, sc2::ABILITY_ID::HARVEST_RETURN);
					continue;
				}
			}
			else {
				//Micro::
				workers[i].SCVstate = MovingToMineral;
				Micro::SmartRightClick(workers[i].unit, workers[i].mineral, m_bot);
				//m_bot.Actions()->UnitCommand(unit.scv, sc2::ABILITY_ID::HARVEST_RETURN);
				//m_bot.Actions()->UnitCommand( u
			}
		}
	}
}

void BaseManager::onFrameQueuePath()
{
	for (int i = 0; i < workers.size(); i++) {
		std::stringstream ss2;
		ss2 << workers[i].SCVstate; //ss2 << workers[i].scv->mineral_contents;
		std::string facingSTR = ss2.str();
		m_bot.Map().drawText(workers[i].unit->pos, facingSTR);

		if (workers[i].SCVstate == MovingToMineral) {

			if (workers[i].unit->orders.size() > 0) {
				if (workers[i].unit->orders[0].ability_id == sc2::ABILITY_ID::HARVEST_RETURN) {
					continue;
				}
			}

			if (workers[i].mineral == nullptr) {
				ScheduleWorker(i);
				Micro::SmartRightClick(workers[i].unit, workers[i].mineral, m_bot);
				//continue;
			} //Util::Dist(unit.scv->pos, unit.mineral->pos)

			float maxDist = (float)1.7;
			//if close enough to a mineral, start gathering from it
			if (Util::Dist(workers[i].unit->pos, workers[i].mineral->pos) < maxDist) { //std::cerr << "changing state";
				workers[i].SCVstate = GatheringMineral;
				workers[i].StartedMining = current_frame;

				Micro::SmartRightClick(workers[i].unit, workers[i].mineral, m_bot);
				continue;
			}

			if (workers[i].interMineral != nullptr) {

				float maxDistInter = (float)1.5; //if close enough to the inter mineral, start gathering from the real one
				if (Util::Dist(workers[i].unit->pos, workers[i].interMineral->pos) < maxDistInter) {
					workers[i].SCVstate = GatheringMineral;
					Micro::SmartRightClick(workers[i].unit, workers[i].mineral, m_bot);
					continue;
				}

				if (workers[i].unit->orders[0].target_unit_tag != workers[i].interMineral->tag) {
					Micro::SmartRightClick(workers[i].unit, workers[i].interMineral, m_bot);
					continue;
				}
			}
			if (workers[i].Intermediate != sc2::Point3D(0, 0, 0)) {

				float maxDistInterPoint = (float)1.5; //if close enough to the inter point, start gathering from the real one
				if (Util::Dist(workers[i].unit->pos, workers[i].Intermediate) < maxDistInterPoint) {
					workers[i].SCVstate = GatheringMineral;
					Micro::SmartRightClick(workers[i].unit, workers[i].mineral, m_bot);
					continue;
				}

				const sc2::Point2D  interpoint2d(workers[i].Intermediate);
				if (workers[i].unit->orders.size() > 0) {
					if (workers[i].unit->orders[0].target_pos.x != interpoint2d.x
						&& workers[i].unit->orders[0].target_pos.y != interpoint2d.y
						) {
						Micro::SmartMove(workers[i].unit, workers[i].Intermediate, m_bot);
						continue;
					}
				}
				else {
					Micro::SmartMove(workers[i].unit, workers[i].Intermediate, m_bot);
					continue;
				}
			}
		}


		if (workers[i].SCVstate == GatheringMineral) { //std::cerr << "gathering mineral";

			if (workers[i].mineral == nullptr) {
				workers[i].SCVstate = MovingToMineral;
				std::cerr << "!!!!!!! no mineral at Gathering Mineral";
			}

			if (workers[i].unit->orders.size() > 0) {
				if (workers[i].unit->orders[0].ability_id == sc2::ABILITY_ID::HARVEST_RETURN) {
					workers[i].SCVstate = ReturningMineral;
					RemoveWorkerSchedule(i);

					continue;
				} //keep worker unit at its mineral field
				if ((workers[i].unit->orders[0].ability_id == sc2::ABILITY_ID::HARVEST_GATHER
					)
					&&
					workers[i].unit->orders[0].target_unit_tag != workers[i].mineral->tag
					) {
					Micro::SmartRightClick(workers[i].unit, workers[i].mineral, m_bot);
					continue;
				}
			}
			else {
				Micro::SmartRightClick(workers[i].unit, workers[i].mineral, m_bot);
				continue;
			}

		}

		if (workers[i].SCVstate == ReturningMineral) { //std::cerr << "return mineral";
			//workers[i].SCVstate = MovingToMineral;
			//m_bot.Actions()->UnitCommand(workers[i].scv, sc2::ABILITY_ID::HARVEST_RETURN);
			//continue;

			if (workers[i].unit->orders.size() > 0) {
				if (workers[i].unit->orders[0].ability_id != sc2::ABILITY_ID::HARVEST_RETURN) {
					workers[i].SCVstate = MovingToMineral;
					//Micro::SmartRightClick(unit.scv, unit.mineral, m_bot);
					m_bot.Actions()->UnitCommand(workers[i].unit, sc2::ABILITY_ID::HARVEST_RETURN);
					continue;
				}
			}
			else {
				workers[i].SCVstate = MovingToMineral;
				continue;
				//let scheduler reschedule
				//Micro::SmartRightClick(workers[i].scv, workers[i].mineral, m_bot);
				//m_bot.Actions()->UnitCommand(unit.scv, sc2::ABILITY_ID::HARVEST_RETURN);
			}
		}
	}
}

void BaseManager::addWorkerBuiltIn(const sc2::Unit * unit)
{ //std::cerr << "addWorkerBuiltIn";

	SCV newSCV;
	newSCV.unit = unit;
	newSCV.StartedMoving = 0;
	newSCV.StartedMining = 0;
	newSCV.SCVstate = MovingToMineral;
	if (minerals.size() > 0) {
		newSCV.mineral = minerals[0].mineralUnit;
		Micro::SmartRightClick(unit, minerals[0].mineralUnit, m_bot);
	}
	else { std::cerr << "No minerals"; }

	workers.push_back(newSCV);
}

void BaseManager::addWorkerSplit(const sc2::Unit * unit)
{
	SCV newSCV;
	newSCV.unit = unit;
	newSCV.StartedMoving = 0;
	newSCV.StartedMining = 0;
	newSCV.SCVstate = MovingToMineral; //newSCV.mineral = minerals[0].mineralUnit;

	int choiceMineral = -1;
	int lowestCount = 99;
	for (int i = 0; i < minerals.size(); i++) {
		if (minerals[i].SCVcount < lowestCount) {
			lowestCount = minerals[i].SCVcount;
			choiceMineral = i;
		}
	}
	if (choiceMineral != -1) {
		minerals[choiceMineral].SCVcount++;
		newSCV.mineral = minerals[choiceMineral].mineralUnit;
		newSCV.interMineral = minerals[choiceMineral].mineralTrick;
		newSCV.Intermediate = minerals[choiceMineral].posTrick;
	}

	workers.push_back(newSCV);
}

void BaseManager::addWorkersQueue(const sc2::Unit * unit)
{
	SCV newSCV;
	newSCV.unit = unit;
	newSCV.StartedMoving = 0;
	newSCV.StartedMining = 0;
	newSCV.SCVstate = MovingToMineral;

	//newSCV.mineral = minerals[0].mineralUnit;

	//Dont choose a mineral for them just yet.
	//The queue scheduler will do that for them
	newSCV.mineral = nullptr;

	workers.push_back(newSCV);
}


//find the CLOSEST mineral that has LEAST workers
//add 2 to closest-minerals first
//closest-mineral should take closest-worker, until all minerals have 1 worker
//then repeat until all minerals have 2 workers
//then give 3rd workers to farthest-minerals first
//really, it should go to the best "mineral-increase-delta-for a new worker on this crystal" -- (total-roundtrip - mining-time * current_workers) -- will be '1' if not paired yet / under-saturated. will be a fraction if over-saturated (largest for longer-away)

//multiply delta / round-trip-time //  1/80 will be greater than 1/120, but 0.3/80 will be less than 0.6/120
//must calculate their (distance-to-TownHall) into a (roundtrip-frames) number, than a (mining-delta) from (modulo/leftover of (roundtrip-frames / mining-time) = max-worker-load //roundtrip = 80, mining
void BaseManager::addWorkersPath(const sc2::Unit * unit, const sc2::Unit* townHall)
{
	SCV newSCV;
	newSCV.unit = unit; //newSCV.StartedMoving = 0; //clear Moving start //newSCV.StartedMining = 0; //clear Mining start
	newSCV.SCVstate = MovingToMineral; //getting a little ahead of ourselves. assuming all bases aren't saturated.

	//sort mineral-crystals by closeness-to-townHall-center, so we can use .back() and .pop_back()
	/*if (!minerals.empty() && townHall != nullptr) {	//const Unit* mineral_target = FindNearestMineralPatch(nexus->pos);
		auto mySortFunctor = [townHall](Mineral a, Mineral b) { return Util::Dist(townHall->pos, a.mineralUnit->pos) < Util::Dist(townHall->pos, b.mineralUnit->pos); };
		std::sort(minerals.begin(), minerals.end(), mySortFunctor );
	}*/
	
	int choiceMineral = -1; //initialize which mineral we want to go to

	int lowestCount = 99; //initialize lowest SCV count of minerals
	float lowestDist = 999.0f; //find closest mineral to SCV

	for (int i = 0; i < minerals.size(); i++) { //go thru all minerals to find least-saturated one
		float dist = Util::Dist(minerals[i].mineralUnit->pos, newSCV.unit->pos);
		if (dist < lowestDist && minerals[i].SCVcount < 2) {
			//lowestCount = minerals[i].SCVcount;
			lowestDist = dist;
			choiceMineral = i;
		}
	}
	/*if (choiceMineral == -1) {
		for (int i = 0; i < minerals.size(); i++) { //go thru all minerals to find least-saturated one
			float dist = Util::Dist(minerals[i].mineralUnit->pos, newSCV.unit->pos);
			if (dist < lowestDist && minerals[i].SCVcount < 2) {
				//lowestCount = minerals[i].SCVcount;
				lowestDist = dist;
				choiceMineral = i;
			}
		}
	}*/

	/*for (int i = 0; i < minerals.size(); i++) { //go thru all minerals to find least-saturated one
		if (minerals[i].SCVcount < lowestCount) {
			lowestCount = minerals[i].SCVcount;
			choiceMineral = i;
		}
	}*/

	if (choiceMineral != -1) {

		minerals[choiceMineral].SCVcount++;
		newSCV.mineral = minerals[choiceMineral].mineralUnit;
		newSCV.interMineral = minerals[choiceMineral].mineralTrick;
		newSCV.Intermediate = minerals[choiceMineral].posTrick;

		Micro::SmartRightClick(newSCV.unit, newSCV.mineral, m_bot);
		//SmartMove(newSCV.scv, newSCV.posTrick, m_bot);
	}

	workers.push_back(newSCV);
}

void BaseManager::addWorkersQueuePath(const sc2::Unit * unit)
{
	addWorkersQueue( unit); //same as  the regular queue
}


void BaseManager::addWorkersSingle(const sc2::Unit * unit)
{
	//find closest mineral to base
	SCV newSCV;
	newSCV.unit = unit;
	newSCV.StartedMoving = 0; //clear Moving start
	newSCV.StartedMining = 0; //clear Mining start
	newSCV.SCVstate = MovingToMineral; //getting a little ahead of ourselves. assuming all bases aren't saturated.

									   //newSCV.mineral = minerals[0].mineralUnit;
	int choiceMineral = -1;
	float closestToTownHall = 999.0f;
	for (int i = 0; i < minerals.size(); i++) { //go thru all minerals to find least-saturated one
		if (minerals[i].SCVcount < 5) {
			closestToTownHall = 1.0f;// minerals[i].SCVcount;
			choiceMineral = i;
		}
	}
	if (choiceMineral != -1) {

		minerals[choiceMineral].SCVcount++;
		newSCV.mineral = minerals[choiceMineral].mineralUnit;
		newSCV.interMineral = minerals[choiceMineral].mineralTrick;
		newSCV.Intermediate = minerals[choiceMineral].posTrick;

		Micro::SmartRightClick(newSCV.unit, newSCV.mineral, m_bot);
		//SmartMove(newSCV.scv, newSCV.posTrick, m_bot);
	}

	workers.push_back(newSCV);
}

void BaseManager::firstFrameWorkerSpread() {

	auto list = m_bot.Observation()->GetUnits(sc2::Unit::Alliance::Self, sc2::IsUnit(sc2::UNIT_TYPEID::TERRAN_COMMANDCENTER));
	auto commandcenter = list.front();
	auto npos = commandcenter->pos;

	sc2::Units mins = m_bot.Observation()->GetUnits(sc2::Unit::Alliance::Neutral, [npos](const sc2::Unit & u) { return  Util::IsMineral(&u) && Distance2D(u.pos, npos) < 15.0f; });
	sc2::Units scvs = m_bot.Observation()->GetUnits(sc2::Unit::Alliance::Self, [npos](const sc2::Unit & u) { return  u.unit_type == sc2::UNIT_TYPEID::PROTOSS_PROBE && Util::Dist(u.pos, npos) < 15.0f; });

	std::vector<int> targets = allocateMineralWorkers(scvs, mins, commandcenter); //[](const sc2::Unit *u) { return  2; }, commandcenter);

	for (int att = 0, e = (int)targets.size(); att < e; att++) {
		if (targets[att] != -1) {
			//m_bot.Actions()->UnitCommand(scvs[att], sc2::ABILITY_ID::SMART, mins[targets[att]]);

		}
	}

}

//allocate workers to the best mineral fields //the "toAlloc" functor is "how many probes per this mineral-crystal?
std::vector<int> BaseManager::allocateMineralWorkers(const sc2::Units & workers, const sc2::Units & mins, const sc2::Unit* townHall) { //int(*toAlloc) (const sc2::Unit *), const sc2::Unit* townHall, bool keepCurrent = false) {

	bool keepCurrent = false;
	std::unordered_map<sc2::Tag, int> workerTagToIndexMap; //map workers' Unit.unit_tag to their index in the "workers" vector function-parameter
	for (int i = 0, e = (int)workers.size(); i < e; i++) { workerTagToIndexMap.insert_or_assign(workers[i]->tag, i); }
	std::unordered_map<sc2::Tag, int> mineralTagToIndexMap; //map mineral-crystals' Unit.unit_tag to their index in the "mins" vector function-parameter
	for (int i = 0, e = (int)mins.size(); i < e; i++) { mineralTagToIndexMap.insert_or_assign(mins[i]->tag, i); }

	std::vector<int> workerToMineralTargetList; //this is the returned-result, where index is 'workers'-index and value is 'mins'-index, of the passed-in Units lists
	workerToMineralTargetList.resize(workers.size(), -1);

	std::vector<std::vector<int>> assignedWorkersOnMineral; //list of 'workers'-index per each 'mins' mineral-unit
	assignedWorkersOnMineral.resize(mins.size());

	std::vector<int> freeAgents; //workers available to assign to minerals
	std::vector<int> freeMins; //std::remove_if(mins.begin(), mins.end(), [npos](const Unit * u) { return  Distance2D(u->pos, npos) > 6.0f;  });

	if (keepCurrent) { //if function-parameter indicated to keep the workers' current mineral-crystal target
		int i = 0;
		for (const auto & u : workers) {
			if (!u->orders.empty()) { //if worker does have orders already
				const auto & o = u->orders.front(); //get the first order
				if (o.target_unit_tag != 0) { //if target tag is valid
					auto pu = mineralTagToIndexMap.find(o.target_unit_tag); //grab 
					if (pu == mineralTagToIndexMap.end()) {
						workerToMineralTargetList[i] = -1;
					}
					else {
						int ind = pu->second;
						workerToMineralTargetList[i] = ind;
						assignedWorkersOnMineral[ind].push_back(i);
					}
				}
			}
			i++;
		}
		for (int i = 0, e = (int)mins.size(); i < e; i++) {
			auto sz = assignedWorkersOnMineral[i].size();
			int maxGatherersPerCrystal = 2;//toAlloc(mins[i]); //this should return a '2' for close minerals until they are full, then return a '3' for far-minerals, then '3' for close-minerals, depending on total-workers
			if (sz > maxGatherersPerCrystal) {
				auto start = mins[i]->pos;
				std::sort(assignedWorkersOnMineral[i].begin(), assignedWorkersOnMineral[i].end(), [start, workers](int a, int b) { return Util::Dist(start, workers[a]->pos) < Util::Dist(start, workers[b]->pos); });

				//free up if too many workers on one crystal
				for (int j = maxGatherersPerCrystal, e = (int)assignedWorkersOnMineral[i].size(); j < e; j++) {
					freeAgents.push_back(assignedWorkersOnMineral[i][j]);
					workerToMineralTargetList[assignedWorkersOnMineral[i][j]] = -1;
				}
				assignedWorkersOnMineral[i].resize(maxGatherersPerCrystal);

			}
			else if (sz < maxGatherersPerCrystal) {
				freeMins.push_back(i);
			}
		}
	}
	else {
		for (int i = 0, e = (int)workers.size(); i < e; i++) {
			freeAgents.push_back(i);
		}
		for (int i = 0, e = (int)mins.size(); i < e; i++) {
			freeMins.push_back(i);
		}
	}

	//sort mineral-crystals by closeness-to-townHall-center in reverse-order, so we can use .back() and .pop_back()
	if (!freeMins.empty() && townHall != nullptr) {	//const Unit* mineral_target = FindNearestMineralPatch(nexus->pos);
		std::sort(freeMins.begin(), freeMins.end(), [townHall, mins](int a, int b) { return Util::Dist(townHall->pos, mins[a]->pos) > Util::Dist(townHall->pos, mins[b]->pos); });
	}

	//choose closest freeAgent to each mineral-crystal
	while (!freeMins.empty() && !freeAgents.empty()) {
		int chooser = freeMins.back(); //the mineral-crystal gets to choose its miners!
		freeMins.pop_back();
		auto target_pos = mins[chooser]->pos; //this should probably be the magicPosition, for higher chance of avoiding workers behind mineral-line from taking precedence
		std::sort(freeAgents.begin(), freeAgents.end(), [target_pos, workers](int a, int b) { return Util::Dist(target_pos, workers[a]->pos) > Util::Dist(target_pos, workers[b]->pos); });

		int assignee = freeAgents.back();
		freeAgents.pop_back();

		assignedWorkersOnMineral[chooser].push_back(assignee);
		workerToMineralTargetList[assignee] = chooser;
		
		if (assignedWorkersOnMineral[chooser].size() < 2) { //toAlloc(mins[chooser])) {
			freeMins.insert(freeMins.begin(), chooser);
		}
	}

	//go back thru and assign 3rd workers starting from farthest-minerals
	//int maxWorkersPerCrystal = (freeAgents.size() > freeMins.size() * 2) ? 2 : 3; //if more than 2 freeAgents available per crystal, put 3 on closest //best for the 3 to go to far-minerals, but takes more coding

	return workerToMineralTargetList;

}

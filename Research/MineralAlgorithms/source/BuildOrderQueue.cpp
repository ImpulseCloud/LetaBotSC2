#include "BuildOrderQueue.h"
#include "CCBot.h"

BuildOrderQueue::BuildOrderQueue(CCBot & bot)
    : m_bot(bot)
    , m_highestPriority(0)
    , m_lowestPriority(0)
    , m_defaultPrioritySpacing(10)
    , m_numSkippedItems(0)
{

}

void BuildOrderQueue::clearAll()
{
    // clear the queue
    m_queue.clear();

    // reset the priorities
    m_highestPriority = 0;
    m_lowestPriority = 0;
}

BuildOrderItem & BuildOrderQueue::getHighestPriorityItem()
{
    // reset the number of skipped items to zero
    m_numSkippedItems = 0;

    // the queue will be sorted with the highest priority at the back
    return m_queue.back();
}

BuildOrderItem & BuildOrderQueue::getNextHighestPriorityItem()
{
    assert(m_queue.size() - 1 - m_numSkippedItems >= 0);

    // the queue will be sorted with the highest priority at the back
    return m_queue[m_queue.size() - 1 - m_numSkippedItems];
}

void BuildOrderQueue::skipItem()
{
    // make sure we can skip
    assert(canSkipItem());

    // skip it
    m_numSkippedItems++;
}

//i assume this means have enough money to build the next item before the next-in-line is 'started' (building structure with placed-schematic, but not placed-physically) ???
bool BuildOrderQueue::canSkipItem()
{ // does the queue have more elements
    bool bigEnough = m_queue.size() > (size_t)(1 + m_numSkippedItems);

    if (!bigEnough) { return false; }

    // is the current highest priority item not blocking a skip
    bool highestNotBlocking = !m_queue[m_queue.size() - 1 - m_numSkippedItems].blocking;

    return highestNotBlocking; // this tells us if we can skip
}

void BuildOrderQueue::queueItem(const BuildOrderItem & b)
{
    if (m_queue.empty()) { m_highestPriority = b.priority; m_lowestPriority = b.priority; } // if the queue is empty, set the highest and lowest priorities

    if (b.priority <= m_lowestPriority) { m_queue.push_front(b); } else { m_queue.push_back(b); } // push the item into the queue 

    // if the item is somewhere in the middle, we have to sort again
    if ((m_queue.size() > 1)  &&  (b.priority < m_highestPriority)  &&  (b.priority > m_lowestPriority))
    {  std::sort(m_queue.begin(), m_queue.end());  } // sort the list in ascending order, putting highest priority at the top
    
    // update the highest or lowest if it is beaten
    m_highestPriority = (b.priority > m_highestPriority) ? b.priority : m_highestPriority;
    m_lowestPriority  = (b.priority < m_lowestPriority)  ? b.priority : m_lowestPriority;
}

void BuildOrderQueue::queueAsHighestPriority(const BuildType & type, bool blocking)
{
    int newPriority = m_highestPriority + m_defaultPrioritySpacing; // the new priority will be higher
    queueItem(BuildOrderItem(type, newPriority, blocking)); // queue the item
}

void BuildOrderQueue::queueAsLowestPriority(const BuildType & type, bool blocking)
{
    int newPriority = m_lowestPriority - m_defaultPrioritySpacing; // the new priority will be lower
    queueItem(BuildOrderItem(type, newPriority, blocking)); // queue the item
}

void BuildOrderQueue::removeHighestPriorityItem()
{
    m_queue.pop_back(); // remove the back element of the vector
	
    // if the list is not empty, set the highest accordingly
    m_highestPriority = m_queue.empty() ? 0 : m_queue.back().priority;
    m_lowestPriority  = m_queue.empty() ? 0 : m_lowestPriority;
}

void BuildOrderQueue::removeCurrentHighestPriorityItem()
{
    // remove the back element of the vector
    m_queue.erase(m_queue.begin() + m_queue.size() - 1 - m_numSkippedItems);

    //assert((int)(queue.size()) < size);

    // if the list is not empty, set the highest accordingly
    m_highestPriority = m_queue.empty() ? 0 : m_queue.back().priority;
    m_lowestPriority  = m_queue.empty() ? 0 : m_lowestPriority;
}

size_t BuildOrderQueue::size()
{
    return m_queue.size();
}

bool BuildOrderQueue::isEmpty()
{
    return (m_queue.size() == 0);
}

BuildOrderItem BuildOrderQueue::operator [] (int i)
{
    return m_queue[i];
}

std::string BuildOrderQueue::getQueueInformation() const
{
    size_t reps = m_queue.size() < 30 ? m_queue.size() : 30;
    std::stringstream ss;

    // for each unit in the queue
    for (size_t i(0); i<reps; i++)
    {
        const BuildType & type = m_queue[m_queue.size() - 1 - i].type;
        ss << type.getName() << "\n";
    }

    return ss.str();
}


BuildOrderItem::BuildOrderItem(const BuildType & t, int p, bool b)
    : type(t)
    , priority(p)
    , blocking(b)
{
}

bool BuildOrderItem::operator < (const BuildOrderItem & x) const
{
    return priority < x.priority;
}
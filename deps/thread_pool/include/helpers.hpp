#pragma once

template <typename TasksType>
void WaitForAll( TasksType& tasks )
{
    for ( auto& task : tasks )
        task.wait();
}


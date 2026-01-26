#pragma once
#include "engine/project/project.hpp"
#include "engine/utils/timer.hpp"
#include <chrono>

namespace bubble
{

class AutoBackup
{
public:
    AutoBackup( Project& project, float intervalMinutes = 5.0f )
        : mProject( project )
        , mIntervalSeconds( intervalMinutes * 60.0f )
        , mTimeSinceLastBackup( 0.0f )
    {
    }

    void OnUpdate( DeltaTime deltaTime )
    {
        if ( not mProject.IsValid() )
            return;

        mTimeSinceLastBackup += deltaTime;

        if ( mTimeSinceLastBackup >= mIntervalSeconds )
        {
            PerformBackup();
            mTimeSinceLastBackup = 0.0f;
        }
    }

    void SetInterval( float minutes )
    {
        mIntervalSeconds = minutes * 60.0f;
    }

    void ForceBackup()
    {
        if ( mProject.IsValid() )
        {
            PerformBackup();
            mTimeSinceLastBackup = 0.0f;
        }
    }

private:
    void PerformBackup()
    {
        try
        {
            // Get project directory
            path projectDir = mProject.mRootFile.parent_path();
            path backupDir = projectDir / "backups";

            // Create backups directory if it doesn't exist
            if ( not std::filesystem::exists( backupDir ) )
                std::filesystem::create_directories( backupDir );

            // Generate timestamp for backup filename
            auto now = std::chrono::system_clock::now();
            string timestamp = std::format( "{:%Y%m%d_%H%M%S}", now );

            // Create backup filename: project_name_timestamp.bubble
            string backupFilename = std::format( "{}_{}.bubble", mProject.mName, timestamp );
            path backupPath = backupDir / backupFilename;

            // Save current project file to backup
            path originalFile = mProject.mRootFile;
            mProject.mRootFile = backupPath;
            mProject.Save();
            mProject.mRootFile = originalFile;

            // Clean up old backups (keep only the last 10)
            CleanOldBackups( backupDir, 10 );
        }
        catch ( const std::exception& e )
        {
            LogError( std::format( "AutoBackup failed: {}", e.what() ) );
        }
    }

    void CleanOldBackups( const path& backupDir, size_t maxBackups )
    {
        try
        {
            // Collect all backup files
            vector<std::filesystem::directory_entry> backupFiles;
            for ( const auto& entry : std::filesystem::directory_iterator( backupDir ) )
            {
                if ( entry.is_regular_file() && entry.path().extension() == ".bubble" )
                {
                    backupFiles.push_back( entry );
                }
            }

            // Sort by modification time (oldest first)
            std::ranges::sort( backupFiles, []( const auto& a, const auto& b )
            {
                return std::filesystem::last_write_time( a ) < std::filesystem::last_write_time( b );
            } );

            // Delete oldest backups if we exceed the limit
            if ( backupFiles.size() > maxBackups )
            {
                size_t filesToDelete = backupFiles.size() - maxBackups;
                for ( size_t i = 0; i < filesToDelete; ++i )
                {
                    std::filesystem::remove( backupFiles[i].path() );
                }
            }
        }
        catch ( const std::exception& e )
        {
            LogError( std::format( "CleanOldBackups failed: {}", e.what() ) );
        }
    }

    Project& mProject;
    float mIntervalSeconds;
    float mTimeSinceLastBackup;
};

} // namespace bubble

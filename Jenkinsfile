pipeline {

  agent any

  environment {
    BIN_FILE            = 'firmware.bin'
    ENV_LIST            = 'esp32dev wt32-eth01'
    REPO_BASE_URL       = credentials('repository_base_url')
    REPO_CREDS          = credentials('repository-creds')
    PLATFORMIO_CORE_DIR = "${WORKSPACE}/.platformio"
    HOME                = "${WORKSPACE}"
    PIO_IMAGE           = "local/platformio-arm:latest"
  }

  stages {

    stage('Initialize Vars') {
      steps {
        script {
          if (!env.REPO_BASE_URL) {
            error "REPO_BASE_URL (credential 'repository_base_url') is not configured"
          }
          def base = env.REPO_BASE_URL.trim()
          base = base.replaceAll('/+$', '')
          env.REPO_BASE = base

          env.PROJECT  = "ESP32InitialFirmware"
          env.BRANCH   = env.BRANCH_NAME ?: 'unknown'
          env.DATE     = sh(script: "date +%Y%m%d", returnStdout: true).trim()
          env.BUILD_NO = env.BUILD_NUMBER

          env.UPLOAD_DIR = "${env.REPO_BASE}/${env.PROJECT}/${env.BRANCH}/${env.DATE}_${env.BUILD_NO}/"
          // We'll upload one firmware per environment, e.g., firmware-nodemcuv2.bin
        }
      }
    }

    stage('Checkout') {
      steps {
        checkout scm
      }
    }

    stage('Prepare PlatformIO Image (docker)') {
      steps {
        script {
          def imageExists = sh(
            script: "docker image inspect ${env.PIO_IMAGE} >/dev/null 2>&1",
            returnStatus: true
          ) == 0

          if (!imageExists) {
            echo "PlatformIO image ${env.PIO_IMAGE} not found, building..."

            writeFile file: 'Dockerfile.platformio-arm', text: '''FROM python:3.11-slim

# Install basic build deps (git is useful for many PlatformIO projects)
RUN apt-get update && apt-get install -y --no-install-recommends \
      git \
    && rm -rf /var/lib/apt/lists/*

# Install PlatformIO CLI
RUN pip install --no-cache-dir -U platformio

WORKDIR /workspace

# Store PlatformIO data inside workspace so it persists via volume mount
ENV PLATFORMIO_CORE_DIR=/workspace/.platformio \
    PIO_HOME_DIR=/workspace/.platformio

# No ENTRYPOINT override; we'll invoke "pio" explicitly
'''

            sh """
              docker build -t ${env.PIO_IMAGE} -f Dockerfile.platformio-arm .
            """
          } else {
            echo "PlatformIO image ${env.PIO_IMAGE} already exists, reusing."
          }
        }
      }
    }

    stage('Build Firmware (docker)') {
      steps {
        sh """
          docker run --rm \
            --volumes-from jenkins \
            -e PLATFORMIO_CORE_DIR="${WORKSPACE}/.platformio" \
            -e PIO_HOME_DIR="${WORKSPACE}/.platformio" \
            -w "${WORKSPACE}" \
            ${env.PIO_IMAGE} \
            pio run -e esp32dev -e wt32-eth01
        """
      }
    }

    stage('Prepare Upload Directory') {
      steps {
        withCredentials([
          usernamePassword(
            credentialsId: 'repository-creds',
            usernameVariable: 'REPO_USER',
            passwordVariable: 'REPO_PASS'
          )
        ]) {
          sh '''#!/bin/bash
            set -eu

            base="$REPO_BASE"
            dir="$UPLOAD_DIR"

            rel="${dir#$base/}"

            IFS='/' read -r -a parts <<< "$rel"
            path=""
            for p in "${parts[@]}"; do
              [ -z "$p" ] && continue
              path="$path/$p"
              curl -sf -u "$REPO_USER:$REPO_PASS" -X MKCOL "$base$path" || true
            done
          '''
        }
      }
    }

    stage('Upload Firmware') {
      steps {
        withCredentials([
          usernamePassword(
            credentialsId: 'repository-creds',
            usernameVariable: 'REPO_USER',
            passwordVariable: 'REPO_PASS'
          )
        ]) {
          sh '''#!/bin/bash
            set -e

            for env in ${ENV_LIST}; do
              src=".pio/build/${env}/${BIN_FILE}"
              name="firmware-${env}.bin"
              dest="${UPLOAD_DIR}${name}"
              echo "Uploading $src -> $dest"
              curl -fsS -u "$REPO_USER:$REPO_PASS" \
                -X PUT --upload-file "$src" \
                "$dest"
            done
          '''
        }
      }
    }

    stage('Verify Upload') {
      steps {
        withCredentials([
          usernamePassword(
            credentialsId: 'repository-creds',
            usernameVariable: 'REPO_USER',
            passwordVariable: 'REPO_PASS'
          )
        ]) {
          sh '''#!/bin/bash
            set -e
            for env in ${ENV_LIST}; do
              name="firmware-${env}.bin"
              url="${UPLOAD_DIR}${name}"
              echo "Verifying uploaded firmware at $url"
              curl -fsS -u "$REPO_USER:$REPO_PASS" \
                -r 0-0 "$url" > /dev/null
            done
          '''
        }
      }
    }

  }
}

